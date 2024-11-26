/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_spiffs.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "esp_vfs_fat.h"
#include "usb/usb_host.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "bsp/esp32_p4_function_ev_board.h"
#include "blkcache.h"
#include "esp_check.h"
#include "ff.h"
#include "diskio_impl.h"
#include "sdmmc_cmd.h"

static const char *TAG = "ESP32_P4_EV";

static sdmmc_card_t card;
static blkcache_handle_t *bc;
FATFS *fatfs;


static DSTATUS dio_init (unsigned char pdrv) {
	return 0;
}

static DSTATUS dio_status (unsigned char pdrv) {
	return 0;
}

static DRESULT dio_read (unsigned char pdrv, unsigned char *buff, uint32_t sector, unsigned count) {
	blkcache_read_sectors(bc, buff, sector, count);
	return RES_OK;
}

static DRESULT dio_write (unsigned char pdrv, const unsigned char *buff, uint32_t sector, unsigned count) {
	blkcache_write_sectors(bc, buff, sector, count);
	return RES_OK;
}

static DRESULT dio_ioctl (unsigned char pdrv, unsigned char cmd, void *buff) {
	if (cmd==CTRL_SYNC) {
		//todo
	} else if (cmd==GET_SECTOR_COUNT) {
		*((DWORD*) buff) = card.csd.capacity;
	} else if (cmd==GET_SECTOR_SIZE) {
		*((DWORD*) buff) = card.csd.sector_size;
	} else if (cmd==GET_BLOCK_SIZE) {
		return RES_ERROR;
	} else if (cmd==CTRL_TRIM) {
		return RES_ERROR;
	} else {
		return RES_ERROR;
	}
	return RES_OK;
}

void sdcard_mount_blkcache(const char *mountpoint, int files) {
	sd_pwr_ctrl_ldo_config_t ldo_config = {
		.ldo_chan_id = 4,
	};
	sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;
	esp_err_t ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
		return;
	}

	sdmmc_host_t host = SDMMC_HOST_DEFAULT();
	host.slot = SDMMC_HOST_SLOT_0;
//	host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
	host.max_freq_khz =	 SDMMC_FREQ_52M;
	host.pwr_ctrl_handle = pwr_ctrl_handle;
	const sdmmc_slot_config_t slot_config = {
		/* SD card is connected to Slot 0 pins. Slot 0 uses IO MUX, so not specifying the pins here */
		.cd = SDMMC_SLOT_NO_CD,
		.wp = SDMMC_SLOT_NO_WP,
		.width = 4,
		.flags = 0,
	};
	ESP_ERROR_CHECK(sdmmc_host_init());
	ESP_ERROR_CHECK(sdmmc_host_init_slot(host.slot, &slot_config));
	ESP_ERROR_CHECK(sdmmc_card_init(&host, &card));
	sdmmc_card_print_info(stdout, &card);

	blkcache_config_t bcfg={
		.blksize=1024*8,
		.blkcount=8,
		.read_sectors_cb=(read_sectors_t)sdmmc_read_sectors,
		.write_sectors_cb=(write_sectors_t)sdmmc_write_sectors,
		.arg=(void*)&card
	};
	blkcache_init(&bcfg, &bc);

	ff_diskio_impl_t discio={
		.init=dio_init,
		.status=dio_status,
		.read=dio_read,
		.write=dio_write,
		.ioctl=dio_ioctl
	};

	BYTE pdrv = 0xFF;
	if (ff_diskio_get_drive(&pdrv) != ESP_OK) {
		printf("Out of drive numbers\n");
	}
	printf("diskio: pdrv %hhd\n", pdrv);
	ff_diskio_register(pdrv, &discio);

	char drv[3]={'0'+pdrv, ':', 0};
	esp_vfs_fat_conf_t conf = {
		.base_path = mountpoint, //"/sdcard"
		.fat_drive = drv,
		.max_files = files,
	};
	ESP_ERROR_CHECK(esp_vfs_fat_register_cfg(&conf, &fatfs));

	FRESULT fr=f_mount(fatfs, drv, 1);
	if (fr!=FR_OK) {
		printf("f_mount failed %d\n", fr);
	}
}


