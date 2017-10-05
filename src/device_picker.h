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

#ifndef SRC_DEVICE_PICKER_H_
#define SRC_DEVICE_PICKER_H_

#include <QButtonGroup>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QWidget>

#include "device.h"
#include "option.h"
#include "util.h"

namespace gondar {

class DevicePicker : public QWidget {
  Q_OBJECT

 public:
  DevicePicker();

  Option<DeviceGuy> selectedDevice() const;

  void refresh(const DeviceGuyList& devices);

 signals:
  void selectionChanged();

 protected:
  class Button;

  virtual const Button* selectedButton() const;

  void onButtonClicked(QAbstractButton* button);

  QButtonGroup button_group_;
  QVBoxLayout layout_;
};

class DevicePicker::Button : public QRadioButton {
 public:
  Button(const DeviceGuy& device, QWidget* parent)
      : QRadioButton(QString::fromStdString(device.name), parent),
        device_(device) {
    if (device.num_bytes < 6 * gondar::getGigabyte()) {
      setEnabled(false);
      setText(QString::fromStdString(device.name) + " (too small)");
    }
  }

  const DeviceGuy& device() const { return device_; }

 private:
  DeviceGuy device_;
};


}  // namespace gondar

#endif  // SRC_DEVICE_PICKER_H_
