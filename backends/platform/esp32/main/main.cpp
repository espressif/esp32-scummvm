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

#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "posixesp-fs-factory.h"

#define TAG "main"

#include "../../../../../config.h"

#include "common/scummsys.h"

#include "backends/modular-backend.h"
#include "esp-mutex.h"
#include "base/main.h"
#include "backends/saves/default/default-saves.h"
#include "backends/timer/default/default-timer.h"
#include "backends/events/default/default-events.h"
#include "common/config-manager.h"
#include "esp-graphics.h"
#include "esp-mixer.h"
#include "gui/debugger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "usb_hid.h"
#include "hid_keys.h"
#include "mmc.h"

class OSystem_esp32 : public ModularMixerBackend, public ModularGraphicsBackend, Common::EventSource {
public:
	OSystem_esp32(bool silenceLogs);
	virtual ~OSystem_esp32();

	virtual void initBackend();

	virtual bool pollEvent(Common::Event &event);

	virtual Common::MutexInternal *createMutex();
	virtual uint32 getMillis(bool skipRecord = false);
	virtual void delayMillis(uint msecs);
	virtual void getTimeAndDate(TimeDate &td, bool skipRecord = false) const;

	virtual void quit();

	virtual void logMessage(LogMessageType::Type type, const char *message);

	virtual void addSysArchivesToSearchSet(Common::SearchSet &s, int priority);

protected:
	virtual Common::Path getDefaultConfigFileName() override;
	virtual Common::Path getDefaultLogFileName() override;

private:
	timeval _startTime;
	bool _silenceLogs;
	bool _was_touched;
	bool _mousedown_queued;
	Common::Point _last_mouse_pos;
	int64_t _last_ts_time_us;
};

OSystem_esp32::OSystem_esp32(bool silenceLogs) :
	_silenceLogs(silenceLogs) {
	_fsFactory = new POSIXESPFilesystemFactory();
}

OSystem_esp32::~OSystem_esp32() {
}


void OSystem_esp32::initBackend() {
	gettimeofday(&_startTime, 0);

	_timerManager = new DefaultTimerManager();
	_eventManager = new DefaultEventManager(this);
	_savefileManager = new DefaultSaveFileManager();
	EspGraphicsManager *gfx = new EspGraphicsManager();
	_graphicsManager = gfx;
	gfx->init();
	_mixerManager = new EspMixerManager(44100, 4096);
	// Setup and start mixer
	_mixerManager->init();

	ConfMan.registerDefault("extrapath", Common::Path("/sdcard/scummvm/extras/"));
	ConfMan.registerDefault("iconspath", Common::Path("/sdcard/scummvm/icons/"));
	ConfMan.registerDefault("pluginspath", Common::Path("/sdcard/scummvm/plugins/"));
	ConfMan.registerDefault("savepath", Common::Path("/sdcard/scummvm/saves/"));
	ConfMan.registerDefault("themepath", Common::Path("/sdcard/scummvm/themes/"));

	BaseBackend::initBackend();
}

//missing codes: backspace, enter (instead of return)
static int keymap[][3] = {
	{ KEY_ENTER, Common::KEYCODE_RETURN, Common::ASCII_RETURN },
	{ KEY_UP, Common::KEYCODE_UP, 0 },
	{ KEY_DOWN, Common::KEYCODE_DOWN, 0 },
	{ KEY_LEFT, Common::KEYCODE_LEFT, 0 },
	{ KEY_RIGHT, Common::KEYCODE_RIGHT, 0 },
	{ KEY_LEFTSHIFT, Common::KEYCODE_LSHIFT, 0 },
	{ KEY_RIGHTSHIFT, Common::KEYCODE_RSHIFT, 0 },
	{ KEY_LEFTCTRL, Common::KEYCODE_LCTRL, 0 },
	{ KEY_RIGHTCTRL, Common::KEYCODE_RCTRL, 0 },
	{ KEY_LEFTALT, Common::KEYCODE_LALT, 0 },
	{ KEY_RIGHTALT, Common::KEYCODE_RALT, 0 },
	{ KEY_LEFTMETA, Common::KEYCODE_LMETA, 0 },
	{ KEY_RIGHTMETA, Common::KEYCODE_RMETA, 0 },
	{ KEY_KP0, Common::KEYCODE_KP0, '0' },
	{ KEY_KP1, Common::KEYCODE_KP1, '1' },
	{ KEY_KP2, Common::KEYCODE_KP2, '2' },
	{ KEY_KP3, Common::KEYCODE_KP3, '3' },
	{ KEY_KP4, Common::KEYCODE_KP4, '4' },
	{ KEY_KP5, Common::KEYCODE_KP5, '5' },
	{ KEY_KP6, Common::KEYCODE_KP6, '6' },
	{ KEY_KP7, Common::KEYCODE_KP7, '7' },
	{ KEY_KP8, Common::KEYCODE_KP8, '8' },
	{ KEY_KP9, Common::KEYCODE_KP9, '9' },
	{ KEY_HOME, Common::KEYCODE_HOME, 0 },
	{ KEY_INSERT, Common::KEYCODE_INSERT, 0 },
	{ KEY_END, Common::KEYCODE_END, 0 },
	{ KEY_PAGEUP, Common::KEYCODE_PAGEUP, 0 },
	{ KEY_PAGEDOWN, Common::KEYCODE_PAGEDOWN, 0 },
	{ KEY_F1, Common::KEYCODE_F1, Common::ASCII_F1 },
	{ KEY_F2, Common::KEYCODE_F2, Common::ASCII_F2 },
	{ KEY_F3, Common::KEYCODE_F3, Common::ASCII_F3 },
	{ KEY_F4, Common::KEYCODE_F4, Common::ASCII_F4 },
	{ KEY_F5, Common::KEYCODE_F5, Common::ASCII_F5 },
	{ KEY_F6, Common::KEYCODE_F6, Common::ASCII_F6 },
	{ KEY_F7, Common::KEYCODE_F7, Common::ASCII_F7 },
	{ KEY_F8, Common::KEYCODE_F8, Common::ASCII_F8 },
	{ KEY_F9, Common::KEYCODE_F9, Common::ASCII_F9 },
	{ KEY_F10, Common::KEYCODE_F10, Common::ASCII_F10 },
	{ KEY_F11, Common::KEYCODE_F11, Common::ASCII_F11 },
	{ KEY_F12, Common::KEYCODE_F12, Common::ASCII_F12 },
	{ KEY_F13, Common::KEYCODE_F13, 0 },
	{ KEY_F14, Common::KEYCODE_F14, 0 },
	{ KEY_F15, Common::KEYCODE_F15, 0 },
	{ KEY_A, Common::KEYCODE_a, 'a' },
	{ KEY_B, Common::KEYCODE_b, 'b' },
	{ KEY_C, Common::KEYCODE_c, 'c' },
	{ KEY_D, Common::KEYCODE_d, 'd' },
	{ KEY_E, Common::KEYCODE_e, 'e' },
	{ KEY_F, Common::KEYCODE_f, 'f' },
	{ KEY_G, Common::KEYCODE_g, 'g' },
	{ KEY_H, Common::KEYCODE_h, 'h' },
	{ KEY_I, Common::KEYCODE_i, 'i' },
	{ KEY_J, Common::KEYCODE_j, 'j' },
	{ KEY_K, Common::KEYCODE_k, 'k' },
	{ KEY_L, Common::KEYCODE_l, 'l' },
	{ KEY_M, Common::KEYCODE_m, 'm' },
	{ KEY_N, Common::KEYCODE_n, 'n' },
	{ KEY_O, Common::KEYCODE_o, 'o' },
	{ KEY_P, Common::KEYCODE_p, 'p' },
	{ KEY_Q, Common::KEYCODE_q, 'q' },
	{ KEY_R, Common::KEYCODE_r, 'r' },
	{ KEY_S, Common::KEYCODE_s, 's' },
	{ KEY_T, Common::KEYCODE_t, 't' },
	{ KEY_U, Common::KEYCODE_u, 'u' },
	{ KEY_V, Common::KEYCODE_v, 'v' },
	{ KEY_W, Common::KEYCODE_w, 'w' },
	{ KEY_X, Common::KEYCODE_x, 'x' },
	{ KEY_Y, Common::KEYCODE_y, 'y' },
	{ KEY_Z, Common::KEYCODE_z, 'z' },
	{ KEY_ESC, Common::KEYCODE_ESCAPE, Common::ASCII_ESCAPE},
	{ 0, 0, 0 }
};


bool OSystem_esp32::pollEvent(Common::Event &event) {
	((DefaultTimerManager *)getTimerManager())->checkTimers();
	((EspMixerManager *)_mixerManager)->updateAudio();


	if (_mousedown_queued) {
		event.type = Common::EVENT_LBUTTONDOWN;
		_mousedown_queued=false;
		return true;
	}

	if (esp_timer_get_time()-_last_ts_time_us>(1000000/60)) {
		_last_ts_time_us=esp_timer_get_time();
		Common::Point pos;
		EspGraphicsManager *gfx=(EspGraphicsManager *)_graphicsManager;
		bool touched=gfx->getTouch(pos);
		if (touched) {
			if (!_was_touched) _mousedown_queued=true;
//			ESP_LOGI(TAG, "ts %d,%d", pos.x, pos.y);
			_was_touched = true;
			event.type = Common::EVENT_MOUSEMOVE;
			event.mouse = pos;
			_last_mouse_pos = pos;
			return true;
		} else if (!touched && _was_touched) {
//			ESP_LOGI(TAG, "ts up");
			_was_touched = false;
			event.type = Common::EVENT_LBUTTONUP;
			event.mouse = _last_mouse_pos;
			return true;
		}
	}

	hid_ev_t ev;
	if (usb_hid_receive_hid_event(&ev)) {
		if (ev.type==HIDEV_EVENT_KEY_DOWN || ev.type==HIDEV_EVENT_KEY_UP) {
			if (ev.type==HIDEV_EVENT_KEY_DOWN) event.type=Common::EVENT_KEYDOWN;
			if (ev.type==HIDEV_EVENT_KEY_UP) event.type=Common::EVENT_KEYUP;
			int i = 0;
			while (keymap[i][0] != 0) {
				if (keymap[i][0] == ev.key.keycode) {
					event.kbd.keycode = static_cast<Common::KeyCode>(keymap[i][1]);
					event.kbd.ascii = keymap[i][2];
					//event.kbd.flags |= Common::KBD_SHIFT; _CTRL; _ALT;
					return true;
				}
				i++;
			}
		}
	}
	return false;
}

Common::MutexInternal *OSystem_esp32::createMutex() {
	return createEspMutexInternal();
}

uint32 OSystem_esp32::getMillis(bool skipRecord) {
	timeval curTime;

	gettimeofday(&curTime, 0);

	return (uint32)(((curTime.tv_sec - _startTime.tv_sec) * 1000) +
			((curTime.tv_usec - _startTime.tv_usec) / 1000));
}

void OSystem_esp32::delayMillis(uint msecs) {
	vTaskDelay(pdMS_TO_TICKS(msecs));
}

void OSystem_esp32::getTimeAndDate(TimeDate &td, bool skipRecord) const {
	time_t curTime = time(0);
	struct tm t = *localtime(&curTime);
	td.tm_sec = t.tm_sec;
	td.tm_min = t.tm_min;
	td.tm_hour = t.tm_hour;
	td.tm_mday = t.tm_mday;
	td.tm_mon = t.tm_mon;
	td.tm_year = t.tm_year;
	td.tm_wday = t.tm_wday;
}

void OSystem_esp32::quit() {
	exit(0);
}

void OSystem_esp32::logMessage(LogMessageType::Type type, const char *message) {
	if (_silenceLogs)
		return;

	FILE *output = 0;

	if (type == LogMessageType::kInfo || type == LogMessageType::kDebug)
		output = stdout;
	else
		output = stderr;

	fputs(message, output);
	fflush(output);
}

void OSystem_esp32::addSysArchivesToSearchSet(Common::SearchSet &s, int priority) {
	s.add("engine-data", new Common::FSDirectory("/sdcard/scummvm/", 4), priority);
	s.add("gui/themes", new Common::FSDirectory("/sdcard/scummvm/", 4), priority);
}

Common::Path OSystem_esp32::getDefaultConfigFileName() {
	return "/sdcard/scummvm/scummvm.ini";
}

Common::Path OSystem_esp32::getDefaultLogFileName() {
	return "/sdcard/scummvm/scummvm.log";
}

OSystem *OSystem_esp32_create(bool silenceLogs) {
	return new OSystem_esp32(silenceLogs);
}

extern "C" {

void main_task(void *param) {
	// Invoke the actual ScummVM main entry point:
//	const char *argv[]={"scummvm", "-d", "11"};
	const char *argv[]={"scummvm"};
	int res = scummvm_main(sizeof(argv)/sizeof(argv[0]), argv);
	ESP_LOGW(TAG, "Scummvm_main done");
	g_system->destroy();
}

void usbhidTaskStub(void *param) {
	usb_hid_task();
}


int app_main() {
//	bsp_sdcard_mount();
	sdcard_mount_blkcache("/sdcard", 15);

	g_system = OSystem_esp32_create(false);
	assert(g_system);

	xTaskCreatePinnedToCore(usbhidTaskStub, "usbhid", 4096, NULL, 7, NULL, 1);


	int stack_depth=512*1024;
	StaticTask_t *taskbuf=(StaticTask_t*)calloc(1, sizeof(StaticTask_t));
	uint8_t *stackbuf=(uint8_t*)calloc(stack_depth, 1);
	xTaskCreateStaticPinnedToCore(main_task, "main", stack_depth, NULL, 2, (StackType_t*)stackbuf, taskbuf, 0);
	return 0;
}

} //extern c