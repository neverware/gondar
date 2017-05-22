#include <QtGlobal>

// TODO(nicholasbishop): move the extern C to the header file
extern "C" {
#include "gondar.h"
}

DeviceGuyList* GetDeviceList() {
  auto* list = DeviceGuyList_init();
  DeviceGuyList_append(list, 0, "stubdevice0");
  DeviceGuyList_append(list, 1, "stubdevice1");
  return list;
}

void Install(DeviceGuy * target_device, char * image_path) {
  Q_UNUSED(target_device);
  Q_UNUSED(image_path);
}

BOOL IsCurrentProcessElevated() {
  return true;
}
