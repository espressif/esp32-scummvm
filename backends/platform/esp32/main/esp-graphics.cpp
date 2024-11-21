/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define FORBIDDEN_SYMBOL_ALLOW_ALL


#include "common/config-manager.h"
#include "common/str.h"
#include "common/textconsole.h"	// for warning() & error()
#include "common/translation.h"
#include "engines/engine.h"
#include "graphics/blit.h"
#include "gui/ThemeEngine.h"
#include "esp-graphics.h"
#include "bsp/esp-bsp.h"
#include "soc/mipi_dsi_bridge_struct.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#define TAG "EspGraphics"

#define SCREEN_WIDTH    BSP_LCD_H_RES
#define SCREEN_HEIGHT   BSP_LCD_V_RES

void EspGraphicsManager::init() {
	//initialize LCD
	bsp_display_new(NULL, &_panel_handle, &_io_handle);
	esp_lcd_panel_disp_on_off(_panel_handle, true);
	bsp_display_brightness_init();
	bsp_display_brightness_set(100);
	ppa_client_config_t ppa_cfg={
		.oper_type=PPA_OPERATION_SRM,
	};
	ESP_ERROR_CHECK(ppa_register_client(&ppa_cfg, &_ppa));
	_overlay.create(SCREEN_WIDTH, SCREEN_HEIGHT, getOverlayFormat());
	ESP_ERROR_CHECK(bsp_touch_new(NULL, &_touch_handle));
}


void EspGraphicsManager::initSize(uint width, uint height, const Graphics::PixelFormat *format) {
	ESP_LOGI(TAG, "EspGraphicsManager::initSize %d %d", width, height);
	_width = width;
	_height = height;
	_format = format ? *format : Graphics::PixelFormat::createFormatCLUT8();
	_surf.free();
	_surf.create(width, height, _format);
}

Graphics::Surface *EspGraphicsManager::lockScreen() {
	ESP_LOGI(TAG, "EspGraphicsManager::lockScreen");
	return &_surf;
}

void EspGraphicsManager::unlockScreen() {
	ESP_LOGI(TAG, "EspGraphicsManager::unlockScreen");

}

void EspGraphicsManager::updateScreen() {
	//limit to 60fps
	if ((esp_timer_get_time()-_last_time_updated)<(1000000/60)) return;
	_last_time_updated=esp_timer_get_time();

//	ESP_LOGI(TAG, "EspGraphicsManager::updateScreen ovl %d", _overlayVisible);

	uint16_t *lcdbuf;
 	ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(_panel_handle, 1, (void**)&lcdbuf));
	if (_overlayVisible) {
		memcpy(lcdbuf, _overlay->getPixels(), SCREEN_WIDTH*SCREEN_HEIGHT*sizeof(uint16_t));
	}
	ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(_panel_handle, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, lcdbuf));
}

void EspGraphicsManager::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) {
	ESP_LOGI(TAG, "EspGraphicsManager::copyRectToScreen");
//	uint16_t *lcdbuf;
//	ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(panel_handle, 1, (void**)&lcdbuf));

/*
	uint16_t *lcdbuf;
	ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(panel_handle, 1, (void**)&lcdbuf));

	ppa_srm_oper_config_t op={
		.in={
			.buffer=buf,
			.pic_w=pitch/2,
			.pic_h=pitch/2, //assumption
			.block_offset_x=x
			.block_offset_y=y
			.block_w=w,
			.block_h=h,
			.srm_cm=PPA_SRM_COLOR_MODE_RGB565,
		},
		.out={
			.buffer=lcdbuf,
			.buffer_size=SCREEN_WIDTH*SCREEN_HEIGHT*sizeof(int16_t),
			.pic_w=SCREEN_WIDTH,
			.pic_h=SCREEN_HEIGHT,
			.srm_cm=PPA_SRM_COLOR_MODE_RGB565,
		},
		.scale_x=(float)SCREEN_WIDTH/(float)FB_WIDTH,
		.scale_y=(float)SCREEN_HEIGHT/(float)FB_HEIGHT,
		.mode=PPA_TRANS_MODE_BLOCKING,
	};
	ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(_ppa, &op));
	ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(_panel_handle, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, lcdbuf));
*/
}

void EspGraphicsManager::beginGFXTransaction() {
	ESP_LOGI(TAG, "EspGraphicsManager::beginGFXTransaction");
}

OSystem::TransactionError EspGraphicsManager::endGFXTransaction() {
	ESP_LOGI(TAG, "EspGraphicsManager::endGFXTransaction");
	return OSystem::kTransactionSuccess;
}

void EspGraphicsManager::setPalette(const byte *colors, uint start, uint num) {
	ESP_LOGI(TAG, "EspGraphicsManager::setPalette");
}

void EspGraphicsManager::grabPalette(byte *colors, uint start, uint num) const {
	ESP_LOGI(TAG, "EspGraphicsManager::grabPalette");
}

void EspGraphicsManager::copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) {
	ESP_LOGI(TAG, "EspGraphicsManager::copyRectToOverlay");
	_overlay->copyRectToSurface(buf, pitch, x, y, w, h);
}

void EspGraphicsManager::grabOverlay(Graphics::Surface &surface) const {
	const byte *src = (const byte *)_overlay;
	byte *dst = (byte *)surface.getPixels();
	Graphics::copyBlit(dst, src, surface.pitch,
		SCREEN_WIDTH*sizeof(uint16_t), SCREEN_WIDTH, SCREEN_HEIGHT, sizeof(uint16_t));
	ESP_LOGI(TAG, "EspGraphicsManager::grabOverlay");
}

int16 EspGraphicsManager::getOverlayHeight() const {
	return SCREEN_HEIGHT;
}

int16 EspGraphicsManager::getOverlayWidth() const {
	return SCREEN_WIDTH;
}

void EspGraphicsManager::clearOverlay() {
	ESP_LOGI(TAG, "EspGraphicsManager::clearOverlay");
	//should actually copy game screen to overlay...
	memset((void*)_overlay, 0, SCREEN_WIDTH*SCREEN_HEIGHT*sizeof(uint16_t));
}

bool EspGraphicsManager::getTouch(Common::Point &pos) {
	uint16_t x, y, strength;
	uint8_t num=0;
	esp_lcd_touch_read_data(_touch_handle);
	esp_lcd_touch_get_coordinates(_touch_handle, &x, &y, &strength, &num, 1);
	if (num) {
		if (_overlayVisible) {
			pos.x=x;
			pos.y=y;
		} else {
			pos.x=(x*_width)/SCREEN_WIDTH;
			pos.y=(y*_height)/SCREEN_HEIGHT;
		}
		return true;
	}
	return false;
}

