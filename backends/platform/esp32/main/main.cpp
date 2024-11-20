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

#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "esp_log.h"
#include "bsp/esp-bsp.h"


#define TAG "main"

// We use some stdio.h functionality here thus we need to allow some
// symbols. Alternatively, we could simply allow everything by defining
// FORBIDDEN_SYMBOL_ALLOW_ALL
#define FORBIDDEN_SYMBOL_EXCEPTION_FILE
#define FORBIDDEN_SYMBOL_EXCEPTION_stdout
#define FORBIDDEN_SYMBOL_EXCEPTION_stderr
#define FORBIDDEN_SYMBOL_EXCEPTION_fputs
#define FORBIDDEN_SYMBOL_EXCEPTION_exit
#define FORBIDDEN_SYMBOL_EXCEPTION_time_h

#include "../../../../../config.h"

#include "common/scummsys.h"

#include "backends/modular-backend.h"
#include "backends/mutex/null/null-mutex.h"
#include "base/main.h"
#include "backends/saves/default/default-saves.h"
#include "backends/timer/default/default-timer.h"
#include "backends/events/default/default-events.h"
#include "backends/mixer/null/null-mixer.h"
#include "common/config-manager.h"
#include "esp-graphics.h"
#include "gui/debugger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/*
 * Include header files needed for the getFilesystemFactory() method.
 */
#include "backends/fs/posix/posix-fs-factory.h"

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
};

OSystem_esp32::OSystem_esp32(bool silenceLogs) :
	_silenceLogs(silenceLogs) {
	_fsFactory = new POSIXFilesystemFactory();
}

OSystem_esp32::~OSystem_esp32() {
}


void OSystem_esp32::initBackend() {
	gettimeofday(&_startTime, 0);

	_timerManager = new DefaultTimerManager();
	_eventManager = new DefaultEventManager(this);
	_savefileManager = new DefaultSaveFileManager();
	_graphicsManager = new EspGraphicsManager();
	_mixerManager = new NullMixerManager();
	// Setup and start mixer
	_mixerManager->init();

	ConfMan.registerDefault("extrapath", Common::Path("/sdcard/scummvm/extras/"));
	ConfMan.registerDefault("iconspath", Common::Path("/sdcard/scummvm/icons/"));
	ConfMan.registerDefault("pluginspath", Common::Path("/sdcard/scummvm/plugins/"));
	ConfMan.registerDefault("savepath", Common::Path("/sdcard/scummvm/saves/"));
	ConfMan.registerDefault("themepath", Common::Path("/sdcard/scummvm/themes/"));

	BaseBackend::initBackend();
}

bool OSystem_esp32::pollEvent(Common::Event &event) {
	((DefaultTimerManager *)getTimerManager())->checkTimers();
	((NullMixerManager *)_mixerManager)->update(1);

	return false;
}

Common::MutexInternal *OSystem_esp32::createMutex() {
	return new NullMutexInternal();
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
	const char *argv[]={"scummvm"};
	int res = scummvm_main(1, argv);
	ESP_LOGW(TAG, "Scummvm_main done");
	g_system->destroy();
}



int app_main() {
	bsp_sdcard_mount();

	g_system = OSystem_esp32_create(false);
	assert(g_system);

	int stack_depth=512*1024;

	StaticTask_t *taskbuf=(StaticTask_t*)calloc(1, sizeof(StaticTask_t));
	uint8_t *stackbuf=(uint8_t*)calloc(stack_depth, 1);
	xTaskCreateStaticPinnedToCore(main_task, "main", stack_depth, NULL, 2, (StackType_t*)stackbuf, taskbuf, 0);
	return 0;
}

}