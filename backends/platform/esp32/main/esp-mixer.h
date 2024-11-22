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

#ifndef BACKENDS_MIXER_ESP_H
#define BACKENDS_MIXER_ESP_H

#include "backends/mixer/mixer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"
#include "bsp/esp-bsp.h"
#include "esp_codec_dev_defaults.h"
#include "esp_codec_dev.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"


class EspMixerManager : public MixerManager {
public:
	EspMixerManager(int freq, int bufSize);
	virtual ~EspMixerManager();

	/**
	 * Initialize and setups the mixer
	 */
	virtual void init();

	/**
	 * Pauses the audio system
	 */
	virtual void suspendAudio();

	/**
	 * Resumes the audio system
	 */
	virtual int resumeAudio();

	/**
	 * Updates the audio system
	 */
	void updateAudio();


private:
	static void audioTaskStub(void *arg);
	void audioTask();
	RingbufHandle_t _rb;
	esp_codec_dev_handle_t _spk_codec_dev;


protected:
	int _freq, _bufSize;
};

#endif
