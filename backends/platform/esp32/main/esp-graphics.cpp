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
#include "esp_cpu.h"
#include "esp_heap_caps.h"

#define TAG "EspGraphics"

#define SCREEN_WIDTH    BSP_LCD_H_RES
#define SCREEN_HEIGHT   BSP_LCD_V_RES


void EspGraphicsManager::gfxTaskStub(void *param) {
	EspGraphicsManager *obj=(EspGraphicsManager*)param;
	obj->gfxTask();
}


void EspGraphicsManager::gfxTask() {
	uint16_t *rgbfb=NULL;
	uint16_t pal16[256];
	int rgbfb_w=0;
	int rgbfb_h=0;


	while(1) {
		int fbno=0;
		if (xQueueReceive(_fb_num_q, (void*)(&fbno), portMAX_DELAY)) {
			uint16_t *lcdbuf;
			ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(_panel_handle, 1, (void**)&lcdbuf));
			if (fbno==-1) {
				//draw overlay
				memcpy(lcdbuf, _overlay.getPixels(), SCREEN_WIDTH*SCREEN_HEIGHT*sizeof(uint16_t));
			} else {
				//see if intermediate buffer needs resizing
				if (_surf[fbno].w!=rgbfb_w || _surf[fbno].h!=rgbfb_h) {
					rgbfb_w=_surf[fbno].w;
					rgbfb_h=_surf[fbno].h;
					free(rgbfb);
					rgbfb = (uint16_t*)heap_caps_calloc(rgbfb_w*rgbfb_h, sizeof(uint16_t), MALLOC_CAP_DMA|MALLOC_CAP_SPIRAM);
				}
				//convert palette
				for (int i=0; i<256; i++) {
					int r=_pal[fbno][i*3+0];
					int g=_pal[fbno][i*3+1];
					int b=_pal[fbno][i*3+2];
					pal16[i]=((r>>3)<<11)|((g>>2)<<5)|(b>>3);
				}
				//convert image
				uint8_t *src=(uint8_t*)_surf[fbno].getPixels();
				uint16_t *dst=rgbfb;
				for (int i=0; i<rgbfb_w*rgbfb_h; i++) {
					*dst++=pal16[*src++];
				}

				//scale into lcd memory
				ppa_srm_oper_config_t op={
					.in={
						.buffer=rgbfb,
						.pic_w=(uint32_t)rgbfb_w,
						.pic_h=(uint32_t)rgbfb_h,
						.block_w=(uint32_t)rgbfb_w,
						.block_h=(uint32_t)rgbfb_h,
						.srm_cm=PPA_SRM_COLOR_MODE_RGB565,
					},
					.out={
						.buffer=lcdbuf,
						.buffer_size=SCREEN_WIDTH*SCREEN_HEIGHT*sizeof(int16_t),
						.pic_w=SCREEN_WIDTH,
						.pic_h=SCREEN_HEIGHT,
						.srm_cm=PPA_SRM_COLOR_MODE_RGB565,
					},
					.scale_x=(float)SCREEN_WIDTH/(float)rgbfb_w,
					.scale_y=(float)SCREEN_HEIGHT/(float)rgbfb_h,
					.mode=PPA_TRANS_MODE_BLOCKING,
				};
				ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(_ppa, &op));
			}
			ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(_panel_handle, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, lcdbuf));
			xQueueSend(_fb_ret_q, &fbno, portMAX_DELAY);
		}
	}
}



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

	_cur_fb=0;
	_fb_num_q=xQueueCreate(1, sizeof(int));
	_fb_ret_q=xQueueCreate(1, sizeof(int));
	int fbno=1;
	xQueueSend(_fb_ret_q, &fbno, portMAX_DELAY);
	xTaskCreatePinnedToCore(gfxTaskStub, "gfx", 4096, (void*)this, 6, NULL, 1);
}


void EspGraphicsManager::initSize(uint width, uint height, const Graphics::PixelFormat *format) {
	ESP_LOGI(TAG, "EspGraphicsManager::initSize %d %d", width, height);

	int fbno;
	//Wait until current frame is processed before we change things
	xQueueReceive(_fb_ret_q, (void*)(&fbno), portMAX_DELAY);
	xQueueSend(_fb_ret_q, &fbno, portMAX_DELAY);
	//Gfx task should be idle now.

	_width = width;
	_height = height;
	_format = format ? *format : Graphics::PixelFormat::createFormatCLUT8();
	for (int i=0; i<2; i++) {
		_surf[i].free(); //note not sure if you can do this on an uninitialized surf
		_surf[i].create(width, height, _format);
	}
}

Graphics::Surface *EspGraphicsManager::lockScreen() {
	ESP_LOGI(TAG, "EspGraphicsManager::lockScreen");
	return &_surf[_cur_fb];
}

void EspGraphicsManager::unlockScreen() {
	ESP_LOGI(TAG, "EspGraphicsManager::unlockScreen");
}

void EspGraphicsManager::updateScreen() {
	//limit to 30fps
	if ((esp_timer_get_time()-_last_time_updated)<(1000000/30)) return;
	_last_time_updated=esp_timer_get_time();

	uint64_t t=esp_timer_get_time();

	int fbno;
	if (_overlayVisible) {
		fbno=-1;
	} else {
		fbno=_cur_fb;
		if (fbno==0) _cur_fb=1; else _cur_fb=0;
	}

	xQueueSend(_fb_num_q, &fbno, portMAX_DELAY);
	//wait until the other one is done
	int ret_fbno;
	xQueueReceive(_fb_ret_q, (void*)(&ret_fbno), portMAX_DELAY);
	if (fbno!=-1 && ret_fbno!=_cur_fb) {
		ESP_LOGW(TAG, "Huh, fbno != ret_fbno");
	}
	if (!_overlayVisible) {
		//use fb we're going to display as base of fb we're going to modify next
		memcpy(_surf[_cur_fb].getPixels(), _surf[fbno].getPixels(), _surf[fbno].w*_surf[fbno].h);
		memcpy(_pal[_cur_fb], _pal[fbno], 256*3);
	}
	
	ESP_LOGI(TAG, "EspGraphicsManager::updateScreen took %d us", (int)(esp_timer_get_time()-t));
}

void EspGraphicsManager::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) {
	ESP_LOGI(TAG, "EspGraphicsManager::copyRectToScreen %d,%d size %d,%d", x, y, w, h);
	_surf[_cur_fb].copyRectToSurface(buf, pitch, x, y, w, h);
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
	int p=start*3;
	for (int i=0; i<num*3; i++) {
		if (p < 3*256) {
			_pal[_cur_fb][p++]=colors[i];
		}
	}
}

void EspGraphicsManager::grabPalette(byte *colors, uint start, uint num) const {
	ESP_LOGI(TAG, "EspGraphicsManager::grabPalette");
	for (int i=0; i<num*3; i++) {
		colors[i]=_pal[_cur_fb][start*3+i];
	}
}

void EspGraphicsManager::copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) {
	ESP_LOGI(TAG, "EspGraphicsManager::copyRectToOverlay");
	_overlay.copyRectToSurface(buf, pitch, x, y, w, h);
}

void EspGraphicsManager::grabOverlay(Graphics::Surface &surface) const {
	surface.copyFrom(_overlay);
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
	} else {
		return false;
	}
}

