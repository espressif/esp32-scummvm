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

//This is custom to ESP-IDF as it doesn't show the directories in /. As such,
//this hacks in /sdcard as the root / cwd.

// Re-enable some forbidden symbols to avoid clashes with stat.h and unistd.h.
// Also with clock() in sys/time.h in some macOS SDKs.
#define FORBIDDEN_SYMBOL_EXCEPTION_time_h
#define FORBIDDEN_SYMBOL_EXCEPTION_unistd_h
#define FORBIDDEN_SYMBOL_EXCEPTION_mkdir
#define FORBIDDEN_SYMBOL_EXCEPTION_exit		//Needed for IRIX's unistd.h
#define FORBIDDEN_SYMBOL_EXCEPTION_random
#define FORBIDDEN_SYMBOL_EXCEPTION_srandom

#include "posixesp-fs-factory.h"
#include "backends/fs/posix/posix-fs.h"

#include <unistd.h>

AbstractFSNode *POSIXESPFilesystemFactory::makeRootFileNode() const {
	return new POSIXFilesystemNode("/sdcard/");
}

AbstractFSNode *POSIXESPFilesystemFactory::makeCurrentDirectoryFileNode() const {
	return new POSIXFilesystemNode("/sdcard/");
}

AbstractFSNode *POSIXESPFilesystemFactory::makeFileNodePath(const Common::String &path) const {
	assert(!path.empty());
	return new POSIXFilesystemNode(path);
}
