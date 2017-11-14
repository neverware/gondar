// Copyright 2017 Neverware
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef GONDAR_H
#define GONDAR_H

#include "device.h"
#include "shared.h"

DeviceGuyList GetDeviceList();

// Returns true on success
bool Install(DeviceGuy* target_device,
             const char* image_path,
             int64_t image_size);
bool Format(DeviceGuy* target_device);
bool IsCurrentProcessElevated();

#endif
