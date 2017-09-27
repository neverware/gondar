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

#include <QAbstractButton>

#include "test.h"

#include "src/device_picker.h"

//wip
#include "src/gondarwizard.h"
#include "src/log.h"

// for test objects
#include "src/device_picker.h"

#if defined(Q_OS_WIN)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif

inline void initResource() {
  Q_INIT_RESOURCE(gondarwizard);
}

namespace gondar {

// used by selectedButton() for test flow
const DevicePicker::Button* TestDevicePicker::selectedButton() const {
  // honor user selection in case we figure that bit out
  const QAbstractButton* selected = button_group_.checkedButton();
  if (selected) {
    return dynamic_cast<const Button*>(selected);
  } else {
    // let's see if there's a valid choice
    for (auto button : button_group_.buttons()) {
      if (button->isEnabled()) {
        return dynamic_cast<const Button*>(button);
      }
    }
    // the case in which none were enabled
    return NULL;
  }
}

TestDevicePicker::TestDevicePicker() {
}

TestDeviceSelectPage::TestDeviceSelectPage(QWidget* parent) : DeviceSelectPage(parent) {}

void TestDeviceSelectPage::initializePage() { }

TestWizard::TestWizard(QWidget* parent) : GondarWizard(parent) { }

Private* TestWizard::getPrivate() {
  return p_; 
}

namespace {

QAbstractButton* getDevicePickerButton(DevicePicker* picker, const int index) {
  auto* widget = picker->layout()->itemAt(index)->widget();
  return dynamic_cast<QAbstractButton*>(widget);
}

}  // namespace

uint64_t getValidDiskSize() {
    const uint64_t gigabyte = 1073741824LL;
    return 10 * gigabyte;
}

void Test::testDevicePicker() {
  DevicePicker picker;
  QVERIFY(picker.selectedDevice() == nullopt);

  // Add a single device, does not get auto selected
  picker.refresh({DeviceGuy(1, "a", getValidDiskSize())});
  QVERIFY(picker.selectedDevice() == nullopt);

  // Select the first device
  getDevicePickerButton(&picker, 0)->click();
  QCOMPARE(*picker.selectedDevice(), DeviceGuy(1, "a", getValidDiskSize()));

  // Replace with two new devices
  picker.refresh({DeviceGuy(2, "b", getValidDiskSize()), DeviceGuy(3, "c", getValidDiskSize())});
  QVERIFY(picker.selectedDevice() == nullopt);

  // Select the last device
  auto* btn = getDevicePickerButton(&picker, 1);
  btn->click();

  QCOMPARE(*picker.selectedDevice(), DeviceGuy(3, "c", getValidDiskSize()));
}

void proceed(GondarWizard * wizard) {
  // oh i see.  the last arg is a wait in MILLIseconds
  QTest::mouseClick(wizard->button(QWizard::NextButton), Qt::LeftButton, Qt::NoModifier, QPoint(), 3000);
  LOG_WARNING << "id after click=" << wizard->currentId();
}

void Test::testLinuxStubFlow() {
  initResource();
  gondar::InitializeLogging();
  // let's instead make a mock-wizard?
  // then a mock wizard would make special screens for those that need to be
  // mocked?
  // whatever makes the disk write thread would need a mocked version
  // the download page will have to be mocked 
  //IntegrationTestGondarWizard wizard;
  TestWizard wizard;
  wizard.show();
  // 0->3
  proceed(& wizard);
  // 3->4
  proceed(& wizard);
  // 4->5
  proceed(& wizard);
  // 5->6
  proceed(& wizard);
  // wait a sec'
  QTest::qWait(1000);
  //QVERIFY(wizard.currentId() == 6);
  LOG_WARNING << "curId" << wizard.currentId();
}
}  // namespace gondar

QTEST_MAIN(gondar::Test)
