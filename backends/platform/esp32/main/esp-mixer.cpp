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

#include "common/scummsys.h"

#include "esp-mixer.h"
#include "common/system.h"



#define CHUNKSZ (1024*4)
#define RBSIZE (CHUNKSZ*8)

/*
ToDo: we dump fixed CHUNKSZ sized chunks into a ringbuf... may as well use a queue?

Ideally, the esp_codec thing acquires some sort of will_block functionality
*/

#define DEFAULT_VOLUME 60

void EspMixerManager::audioTaskStub(void *param) {
	EspMixerManager *obj=(EspMixerManager*)param;
	obj->audioTask();
}

void EspMixerManager::audioTask() {
	while(1) {
		size_t size;
		byte *buf=(byte*)xRingbufferReceive(_rb, &size, portMAX_DELAY);
		if (buf) {
			if (size) {
				esp_codec_dev_write(_spk_codec_dev, buf, size);
			}
			vRingbufferReturnItem(_rb, (void*)buf);
		}
	}
}


EspMixerManager::EspMixerManager(int freq, int bufSize)
	:
	_freq(freq),
	_bufSize(bufSize) {

}

EspMixerManager::~EspMixerManager() {
}

void EspMixerManager::init() {
	_mixer = new Audio::MixerImpl(_freq, _bufSize / 4);
	assert(_mixer);

	bsp_i2c_init();
	bsp_audio_init(NULL);
	_spk_codec_dev=bsp_audio_codec_speaker_init();
	esp_codec_dev_set_out_vol(_spk_codec_dev, DEFAULT_VOLUME);
	esp_codec_dev_set_out_mute(_spk_codec_dev, 0);
	esp_codec_dev_sample_info_t fs = {
		.bits_per_sample = 16,
		.channel = 2,
		.channel_mask = 0,
		.sample_rate = (uint32_t)_freq,
		.mclk_multiple = 0,
	};
	esp_codec_dev_open(_spk_codec_dev, &fs);

	_rb=xRingbufferCreate(RBSIZE, RINGBUF_TYPE_NOSPLIT);
	xTaskCreatePinnedToCore(audioTaskStub, "audio", 4096, (void*)this, 7, NULL, 1);

	_mixer->setReady(true);
}

void EspMixerManager::suspendAudio() {
	_audioSuspended = true;
}

int EspMixerManager::resumeAudio() {
	if (!_audioSuspended)
		return -2;

	_audioSuspended = false;
	return 0;
}

void EspMixerManager::updateAudio() {
	byte *buf;
	int tries=0;
	while(1) {
		if (xRingbufferSendAcquire(_rb, (void**)&buf, CHUNKSZ, 0)) {
			if (_audioSuspended) {
				memset((void*)buf, 0, CHUNKSZ);
			} else {
				Audio::MixerImpl *mixer = (Audio::MixerImpl *)g_system->getMixer();
				assert(mixer);
				mixer->mixCallback(buf, CHUNKSZ);
			}
			xRingbufferSendComplete(_rb, (void*)buf);
		} else {
			break;
		}
		tries++;
		if (tries>10) return;
	}
}

