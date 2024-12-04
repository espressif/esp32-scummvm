#include "esp_err.h"

/**
 * @brief Callback type for a backend to write some sectors to storage
 *
 * @param arg Opaque storage object pointer
 * @param dst Destination buffer
 * @param start_sector Sector to start reading from
 * @param sector_count Number of sectors to read
 *
 * @returns ESP_OK or any error
 **/
typedef esp_err_t (*read_sectors_t)(void *arg, void* dst, size_t start_sector, size_t sector_count);

/**
 * @brief Callback type for a backend to write some sectors to storage
 *
 * @param arg Opaque storage object pointer
 * @param src Source buffer
 * @param start_sector Sector to start writing to
 * @param sector_count Number of sectors to write
 *
 * @returns ESP_OK or any error
 **/
typedef esp_err_t (*write_sectors_t)(void *arg, const void* src, size_t start_sector, size_t sector_count);

typedef struct blkcache_handle_t blkcache_handle_t;

/**
 * @brief Blockcache configuration info
 **/
typedef struct {
	size_t blksize;						///< Block size. Needs to be a multiple of 512 bytes.
	size_t blkcount;					///< Amount of blocks in cache
	read_sectors_t read_sectors_cb;		///< Backend read callback
	write_sectors_t write_sectors_cb;	///< Backend write callback
	void *arg;							///< Opaque argument for backend
} blkcache_config_t;


/**
 * @brief Initialize a block cache.
 *
 * @param cfg Blockcache configuration
 * @param ret_handle Blockcache handle pointer
 *
 * @returns ESP_OK or ESP_ERR_NO_MEM in case of no memory.
 */
esp_err_t blkcache_init(const blkcache_config_t *cfg, blkcache_handle_t **ret_handle);

/**
 * @brief Read sectors via the blockcache
 *
 * @param bc Blockcache handle
 * @param src Source buffer
 * @param start_sector Sector to start writing to
 * @param sector_count Number of sectors to write
 *
 * @returns ESP_OK or any error from the backend
 */
esp_err_t blkcache_read_sectors(blkcache_handle_t* bc, void* dst, size_t start_sector, size_t sector_count);


/**
 * @brief Write sectors via the blockcache
 *
 * @param bc Blockcache handle
 * @param src Source buffer
 * @param start_sector Sector to start writing to
 * @param sector_count Number of sectors to write
 *
 * @returns ESP_OK or any error from the backend
 **/
esp_err_t blkcache_write_sectors(blkcache_handle_t* bc, const void* src, size_t start_sector, size_t sector_count);

