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

#ifndef SRC_DEVICE_SELECT_PAGE_H_
#define SRC_DEVICE_SELECT_PAGE_H_

#include <memory>

#include <QLabel>
#include <QVBoxLayout>

#include "device_picker.h"
#include "wizard_page.h"

class DeviceSelectPage : public gondar::WizardPage {
  Q_OBJECT

 public:
  DeviceSelectPage(QWidget* parent = 0);
  DeviceSelectPage(gondar::DevicePicker* picker_in, QWidget* parent = 0);
  void init();
  int nextId() const override;

 protected:
  void initializePage() override;
  bool validatePage() override;
  bool isComplete() const override;

 private:
  QVBoxLayout layout;
  QLabel drivesLabel;
  std::unique_ptr<gondar::DevicePicker> picker;
};

#endif  // SRC_DEVICE_SELECT_PAGE_H_
