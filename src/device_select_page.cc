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

#include "device_select_page.h"

#include "gondarwizard.h"
#include "log.h"

DeviceSelectPage::DeviceSelectPage(QWidget* parent)
    : WizardPage(parent), picker(new gondar::DevicePicker()) {
  // this page should just say 'hi how are you' while it stealthily loads
  // the usb device list.  or it could ask you to insert your device
  init();
}

DeviceSelectPage::DeviceSelectPage(gondar::DevicePicker* picker_in,
                                   QWidget* parent)
    : WizardPage(parent) {
  picker.reset(picker_in);
  init();
}

void DeviceSelectPage::init() {
  setTitle("USB device selection");
  setSubTitle("Choose your target device from the list of devices below.");
  drivesLabel.setText("Select Drive:");
  layout.addWidget(&drivesLabel);
  layout.addWidget(picker.get());
  setLayout(&layout);
  connect(picker.get(), &gondar::DevicePicker::selectionChanged, this,
          &DeviceSelectPage::completeChanged);
}

void DeviceSelectPage::initializePage() {
  picker->refresh(wizard()->usbInsertPage.devices());
}

bool DeviceSelectPage::validatePage() {
  if (const auto device = picker->selectedDevice()) {
    wizard()->writeOperationPage.setDevice(*device);
    return true;
  }
  return false;
}

bool DeviceSelectPage::isComplete() const {
  return picker->selectedDevice() != gondar::nullopt;
}

int DeviceSelectPage::nextId() const {
  if (wizard()->downloadProgressPage.isComplete()) {
    return GondarWizard::Page_writeOperation;
  } else {
    return GondarWizard::Page_downloadProgress;
  }
}
