#include "esp_err.h"

typedef esp_err_t (*read_sectors_t)(void *arg, void* dst, size_t start_sector, size_t sector_count);
typedef esp_err_t (*write_sectors_t)(void *arg, const void* src, size_t start_sector, size_t sector_count);

typedef struct blkcache_handle_t blkcache_handle_t;

typedef struct {
	size_t blksize;
	size_t blkcount;
	read_sectors_t read_sectors_cb;
	write_sectors_t write_sectors_cb;
	void *arg;
} blkcache_config_t;

void blkcache_init(const blkcache_config_t *cfg, blkcache_handle_t **ret_handle);
esp_err_t blkcache_read_sectors(blkcache_handle_t* bc, void* dst, size_t start_sector, size_t sector_count);
esp_err_t blkcache_write_sectors(blkcache_handle_t* bc, const void* src, size_t start_sector, size_t sector_count);
