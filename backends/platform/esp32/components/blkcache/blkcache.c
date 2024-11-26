#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdatomic.h>
#include <string.h>
#include "blkcache.h"
#include "esp_timer.h"

/*
* Task takes op mutex
* Task writes data into req variable
* Task gives 'req' semaphore
* Task takes 'done' semaphore
* blkcache wakes up on taking 'req' semaphore
* blkcache does work
* blkcache sets 'done' sema
* task continues running
*/

//If defined, print out cache hit/miss stats every 5 second
#define SHOW_STATS

//#define DEBUG
#ifdef DEBUG
#define dprintf(...) printf( __VA_ARGS__ )
#else
#define dprintf(...) do{ } while ( 0 )
#endif


#define SECTOR_SIZE 512

#define REQ_TYPE_RESCAN 0
#define REQ_TYPE_READ 1
#define REQ_TYPE_INVALIDATE 2

typedef struct {
	size_t blockno;
	int req_type;
	esp_err_t err; //returned from blkcache task
} blkcache_req_t;

/*
Note on in_use:
- If a reader task sets it, it means any member must be stable (read-only) and that is_read may change.
- If the blkcache task sets it, it means any member of the block can change.
*/
typedef struct {
	atomic_flag in_use; //set if some task is accessing on the rest of the data in this cache block
	size_t blockno;
	bool is_valid;
	bool is_read;
	uint8_t blkdata[];
} blk_t;

typedef struct {
	blkcache_config_t cfg;
	blk_t **blk;
	blkcache_req_t req;
	int fifo_pos;
	SemaphoreHandle_t req_mux;
	SemaphoreHandle_t req_sema;
	SemaphoreHandle_t done_sema;
#ifdef SHOW_STATS
	atomic_int read_ct;
	atomic_int miss_ct;
#endif
} blkcache_t;

//Actually read a block.
static esp_err_t do_read_block(blkcache_t *b, int blkno) {
	//See if we already have this block.
	for (int i=0; i<b->cfg.blkcount; i++) {
		if (b->blk[i]->is_valid && b->blk[i]->blockno==blkno) {
			//Yep, nothing to be done.
			dprintf("do_read_block: block %d already exists in slot %d\n", blkno, i);
			return ESP_OK;
		}
	}
	//First find a block to read into. We use FIFO for cache eviction.
	//Try and lock that block, if that doesn't work move on to the next.
	while(atomic_flag_test_and_set(&b->blk[b->fifo_pos]->in_use)) {
		//Some other task is doing stuff with this block. Just take the next one.
		b->fifo_pos++;
		if (b->fifo_pos>=b->cfg.blkcount) b->fifo_pos=0;
	}
	//If we're here, we have block b->fifo_pos marked in use by us.
	blk_t *blk=b->blk[b->fifo_pos];
	blk->is_valid=false;
	blk->is_read=false;
	blk->blockno=blkno;
	esp_err_t r=b->cfg.read_sectors_cb(b->cfg.arg, blk->blkdata, blkno*(b->cfg.blksize/SECTOR_SIZE), (b->cfg.blksize/SECTOR_SIZE));
	if (r==ESP_OK) {
		blk->is_valid=true;
	} else {
		dprintf("do_read_block: error %d (%s). Not setting slot as valid.\n", r, esp_err_to_name(r));
	}
	atomic_flag_clear(&blk->in_use);
	dprintf("do_read_block: read block %d into slot %d\n", blkno, b->fifo_pos);
	
	//select next slot for next run
	b->fifo_pos++;
	if (b->fifo_pos>=b->cfg.blkcount) b->fifo_pos=0;
	
	return r;
}

void blkcache_task(void *param) {
	blkcache_t *b=(blkcache_t *)param;
	
#ifdef SHOW_STATS
	uint64_t t=esp_timer_get_time();
#endif
	while(1) {
		//Wait for some task to wake us
		dprintf("blkcache_task: idle\n");
		xSemaphoreTake(b->req_sema, portMAX_DELAY);
		if (b->req.req_type == REQ_TYPE_READ) {
			dprintf("blkcache_task: REQ_TYPE_READ %d\n", b->req.blockno);
			esp_err_t r=do_read_block(b, b->req.blockno);
			if (r!=ESP_OK) dprintf("do_read_block: error %d (%s)\n", r, esp_err_to_name(r));
			xSemaphoreGive(b->done_sema);
		} else if (b->req.req_type == REQ_TYPE_INVALIDATE) {
			dprintf("blkcache_task: REQ_TYPE_INVALIDATE %d\n", b->req.blockno);
			for (int i=0; i<b->cfg.blkcount; i++) {
				if (b->blk[i]->is_valid && b->blk[i]->blockno==b->req.blockno) {
					dprintf("blkcache_task: Invalidating cache blk %d for block %d\n", i, b->req.blockno);
					//Wait until the block is not in use. (This rarely should happen.)
					while(atomic_flag_test_and_set(&b->blk[i]->in_use)) vTaskDelay(1);
					b->blk[i]->is_valid=false; //invalidate
					atomic_flag_clear(&b->blk[i]->in_use);
				}
			}
			xSemaphoreGive(b->done_sema);
		} else if (b->req.req_type == REQ_TYPE_RESCAN) {
			xSemaphoreGive(b->done_sema);
		}


		dprintf("blkcache_task: Scan\n");
		//Scan the block cache to see if there are any blocks we need to pre-read
		for (int i=0; i<b->cfg.blkcount; i++) {
			if (b->blk[i]->is_valid && b->blk[i]->is_read) {
				//see if we have the next block as well.
				bool need_preread=true;
				for (int j=0; j<b->cfg.blkcount; j++) {
					if (b->blk[j]->is_valid && b->blk[j]->blockno == b->blk[i]->blockno + 1) {
						need_preread=false;
						break;
					}
				}
				if (need_preread) {
					dprintf("blkcache_task: Scan: preread %d\n", b->blk[i]->blockno + 1);
					esp_err_t r=do_read_block(b, b->blk[i]->blockno + 1);
					if (r!=ESP_OK) dprintf("do_read_block: error %d (%s)\n", r, esp_err_to_name(r));
				}
			}
		}
		dprintf("blkcache_task: Scan done\n");
#ifdef SHOW_STATS
		if (t<esp_timer_get_time() && b->miss_ct!=0) {
			t=esp_timer_get_time()+(1000000*5);
			printf("Blkcache: Cache stats: reads %d misses %d - %.1f pct hits\n", b->read_ct, b->miss_ct, 100.0-((float)b->miss_ct/b->read_ct)*100.0);
		}
#endif
	}
}


void blkcache_init(const blkcache_config_t *cfg, blkcache_handle_t **ret_handle) {
	blkcache_t *b=calloc(1, sizeof(blkcache_t));
	b->blk=calloc(cfg->blkcount, sizeof(blk_t*));
	if (!b->blk) goto err;
	for (int i=0; i<cfg->blkcount; i++) {
		b->blk[i]=calloc(sizeof(blk_t)+cfg->blksize, 1);
		if (!b->blk[i]) goto err;
	}
	b->req_mux=xSemaphoreCreateMutex();
	b->req_sema=xSemaphoreCreateBinary();
	b->done_sema=xSemaphoreCreateBinary();
	memcpy(&b->cfg, cfg, sizeof(blkcache_config_t));
	xTaskCreate(blkcache_task, "blkcache", 4096, b, 2, NULL);
	*ret_handle=(blkcache_handle_t*)b;
	return;
err:
	printf("blkcache: couldn't allocate memory\n");
}

esp_err_t blkcache_read_sectors(blkcache_handle_t* bc, void* dst, size_t start_sector, size_t sector_count) {
	blkcache_t *b=(blkcache_t*)bc;
	esp_err_t r=ESP_OK;
	uint8_t *tgt=(uint8_t*)dst;
	size_t sects_to_read=sector_count;
	bool want_rescan=false;
	while (sects_to_read>0) {
		//todo: off_blk will be 0 after first read; bring out of for loop
		size_t pos_blk=(start_sector)/(b->cfg.blksize/SECTOR_SIZE); //in blocks
		size_t off_blk=((start_sector)%(b->cfg.blksize/SECTOR_SIZE))*SECTOR_SIZE; //in bytes
		dprintf("%p: read blk %d (offset %d), finding in cache\n", dst, pos_blk, off_blk);
		bool found=false;
		for (int i=0; i<b->cfg.blkcount; i++) {
			//go over all (not in use) blocks and copy over 
			if (!atomic_flag_test_and_set(&b->blk[i]->in_use)) {
				dprintf("slot %d is_valid %s blkno %d\n", i, b->blk[i]->is_valid?"yes":"no", b->blk[i]->blockno);
				if (b->blk[i]->is_valid && b->blk[i]->blockno==pos_blk) {
					//Found. Copy the data over to the tgt buffer.
					size_t to_copy_sectors=(b->cfg.blksize-off_blk)/SECTOR_SIZE;
					if (to_copy_sectors>sects_to_read) to_copy_sectors=sects_to_read;
					memcpy(tgt, &b->blk[i]->blkdata[off_blk], to_copy_sectors*SECTOR_SIZE);
					if (!b->blk[i]->is_read) want_rescan=true;
					b->blk[i]->is_read=true; //Mark sector as not pre-read anymore
					//update vars to read next block
					tgt+=b->cfg.blksize-off_blk;
					sects_to_read-=to_copy_sectors;
					start_sector+=to_copy_sectors;
					found=true;
#ifdef SHOW_STATS
					b->read_ct++;
#endif
				}
				atomic_flag_clear(&b->blk[i]->in_use);
				if (found) break;
			} else {
				dprintf("%p: slot %d in use\n", dst, i);
			}
		}
		if (!found) {
#ifdef SHOW_STATS
			b->miss_ct++;
#endif
			//Send work request to reader task.
			dprintf("%p: read blk %d not found in cache, req'ing\n", dst, pos_blk);
			xSemaphoreTake(b->req_mux, portMAX_DELAY);
			b->req.blockno=pos_blk;
			b->req.req_type=REQ_TYPE_READ;
			xSemaphoreGive(b->req_sema);  //wake read task
			xSemaphoreTake(b->done_sema, portMAX_DELAY); //wait till reading is done
			if (b->req.err!=ESP_OK) {
				r=b->req.err;
			}
			//Block should now be in cache. Loop around to fetch it.
			xSemaphoreGive(b->req_mux);
			dprintf("%p: read blk %d not found in cache, req'ing done (%d)\n", dst, pos_blk, r);
			if (r!=ESP_OK) break;
		}
	}
	if (want_rescan) {
		dprintf("%p: req'ing rescan\n", dst);
		//Tell the blkcache task to do a rescan as it may need to preread more data
		xSemaphoreTake(b->req_mux, portMAX_DELAY);
		b->req.req_type=REQ_TYPE_RESCAN;
		xSemaphoreGive(b->req_sema);  //wake read task
		xSemaphoreTake(b->done_sema, portMAX_DELAY); //wait till blkcache task acks
		xSemaphoreGive(b->req_mux);
		dprintf("%p: req'ing rescan done\n", dst);
	}
	return r;
}

/*
Note: this is not exactly great... between writing and invalidating, we can read both old and
new data from the cache. Better to do this per-block atomic, or maybe handle the invalidate-write
in the blkcache task... that also would shield the user from any non-reentrant behaviour.
*/
esp_err_t blkcache_write_sectors(blkcache_handle_t* bc, const void* src, size_t start_sector, size_t sector_count) {
	dprintf("%p: write sect %d size %d\n", src, start_sector, sector_count);
	esp_err_t r=ESP_OK;
	//Do the write
	blkcache_t *b=(blkcache_t*)bc;
	r=b->cfg.write_sectors_cb(b->cfg.arg, src, start_sector, sector_count);
	//Need to invalidate cache for the written range
	xSemaphoreTake(b->req_mux, portMAX_DELAY);
	int pos_blk=start_sector/(b->cfg.blksize/SECTOR_SIZE);
	for (int i=0; i<sector_count; i+=(b->cfg.blksize/SECTOR_SIZE)) {
		b->req.blockno=pos_blk;
		b->req.req_type=REQ_TYPE_INVALIDATE;
		dprintf("%p: invalidate block %d\n", src, pos_blk);
		xSemaphoreGive(b->req_sema);  //wake read task
		xSemaphoreTake(b->done_sema, portMAX_DELAY); //wait till invalidating is done
		dprintf("%p: invalidate block %d done\n", src, pos_blk);
		pos_blk++;
	}
	xSemaphoreGive(b->req_mux);
	dprintf("%p: write sect %d size %d done\n", src, start_sector, sector_count);

	return r;
}



