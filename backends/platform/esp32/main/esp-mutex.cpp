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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "common/scummsys.h"

#include "esp-mutex.h"

class EspMutexInternal final : public Common::MutexInternal {
public:
	EspMutexInternal();
	~EspMutexInternal() override;

	bool lock() override;
	bool unlock() override;

private:
	SemaphoreHandle_t _mutex;
};


EspMutexInternal::EspMutexInternal() {
	_mutex=xSemaphoreCreateRecursiveMutex();
}

EspMutexInternal::~EspMutexInternal() {
	vSemaphoreDelete(_mutex);
}

bool EspMutexInternal::lock() {
	xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
	return true;
}

bool EspMutexInternal::unlock() {
	xSemaphoreGiveRecursive(_mutex);
	return true;
}

Common::MutexInternal *createEspMutexInternal() {
	return new EspMutexInternal();
}

