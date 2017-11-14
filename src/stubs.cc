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

#include <QtGlobal>

#include "gondar.h"

#include "util.h"

DeviceGuyList GetDeviceList() {
  DeviceGuyList devices;
  devices.emplace_back(DeviceGuy(0, "stubdevice0", 10 * gondar::getGigabyte()));
  devices.emplace_back(DeviceGuy(1, "stubdevice1", 4 * gondar::getGigabyte()));
  return devices;
}

bool Install(DeviceGuy* target_device,
             const char* image_path,
             int64_t image_size) {
  Q_UNUSED(target_device);
  Q_UNUSED(image_path);
  Q_UNUSED(image_size);
  return true;
}

bool Format(DeviceGuy* target_device) {
  Q_UNUSED(target_device);
  return true;
}

bool IsCurrentProcessElevated() {
  return true;
}
