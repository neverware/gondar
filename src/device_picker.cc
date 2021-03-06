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

#include "device_picker.h"

#include <QRadioButton>

#include "util.h"

namespace gondar {

DevicePicker::Button::Button(const DeviceGuy& device, QWidget* parent)
    : QRadioButton(QString::fromStdString(device.name), parent),
      device_(device) {
  if (device.num_bytes < 6 * gondar::getGigabyte()) {
    setEnabled(false);
    setText(QString::fromStdString(device.name) + " (too small)");
  }
}

const DeviceGuy& DevicePicker::Button::device() const {
  return device_;
}

DevicePicker::DevicePicker() {
  setLayout(&layout_);

  using OnButtonClicked = void (QButtonGroup::*)(QAbstractButton*);
  connect(&button_group_,
          static_cast<OnButtonClicked>(&QButtonGroup::buttonClicked), this,
          &DevicePicker::onButtonClicked);
}

Option<DeviceGuy> DevicePicker::selectedDevice() const {
  const auto* selected = selectedButton();

  if (selected) {
    return selected->device();
  } else {
    return nullopt;
  }
}

void DevicePicker::refresh(const DeviceGuyList& devices) {
  while (auto* item = layout_.takeAt(0)) {
    auto* button = dynamic_cast<Button*>(item->widget());
    button_group_.removeButton(button);
    delete item;
  }

  for (const auto& device : devices) {
    auto* button = new Button(device, this);
    button_group_.addButton(button);
    layout_.addWidget(button);
  }

  emit selectionChanged();
}

const DevicePicker::Button* DevicePicker::selectedButton() const {
  const QAbstractButton* selected = button_group_.checkedButton();
  return dynamic_cast<const Button*>(selected);
}

void DevicePicker::onButtonClicked(QAbstractButton*) {
  emit selectionChanged();
}

}  // namespace gondar
