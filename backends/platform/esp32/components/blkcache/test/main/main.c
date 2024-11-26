#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "blkcache.h"
#include <assert.h>
#include "esp_timer.h"

#define MAGIC 0x14512523

int magicarg=MAGIC;

esp_err_t read_sectors(int *arg, void* dst, size_t start_sector, size_t sector_count) {
	assert(*arg==MAGIC);
	uint32_t *buf=(uint32_t*)dst;
	for (int i=0; i<sector_count; i++) {
		for (int j=0; j<(512/sizeof(uint32_t)); j++) {
			*buf++=(start_sector+i);
		}
	}
	vTaskDelay(2);
	return ESP_OK;
}

esp_err_t write_sectors(int *arg, const void* src, size_t start_sector, size_t sector_count) {
	assert(*arg==MAGIC);
	//nop
	return ESP_OK;
}

#define MAX_SEC_CT 8

void blktst_task(void *param) {
	printf("Test task start\n");
	blkcache_handle_t *b=(blkcache_handle_t*)param;
	uint32_t *sec_buf=malloc(MAX_SEC_CT*512);
	size_t off=0;
	int t=0;
	srand(esp_timer_get_time());
	while(1) {
		if (rand()&1) {
			//read at new offset
			off=rand()&0xffffff;
		}
		int len=rand()%MAX_SEC_CT;
		blkcache_read_sectors(b, sec_buf, off, len);
//		read_sectors(&magicarg, sec_buf, off, len);
		uint32_t *p=sec_buf;
		for (int i=0; i<len; i++) {
			for (int j=0; j<(512/sizeof(uint32_t)); j++) {
				uint32_t f=*p++;
				if (f!=off+i) {
					printf("Sec %d off %d expected %d (0x%X) found %ld (0x%lX)\n", off+i, j*4, off+i, off+i, f, f);
				}
			}
		}
		off+=len;
		t+=len;
		if (t>100) {
			printf("100 sec read\n");
			t=0;
		}
	}
}


void app_main(void) {
	blkcache_config_t cfg={
		.blksize=1024*8,
		.blkcount=4,
		.read_sectors_cb=(read_sectors_t)read_sectors,
		.write_sectors_cb=(write_sectors_t)write_sectors,
		.arg=(void*)&magicarg
	};
	blkcache_handle_t *h;
	blkcache_init(&cfg, &h);
	for (int i=0; i<5; i++) {
		xTaskCreate(blktst_task, "blktst", 4096, h, 3, NULL);
		vTaskDelay(10);
	}
}
