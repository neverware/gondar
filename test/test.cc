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
#include <QString>

#include "test.h"

#include "src/device.h"
#include "src/device_picker.h"
#include "src/diskwritethread.h"
#include "src/download_progress_page.h"
#include "src/gondarwizard.h"
#include "src/log.h"
#include "src/unzipthread.h"
#include "src/write_operation_page.h"

#if defined(Q_OS_WIN)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif

// test object stuff

namespace {

class MockDevicePicker : public gondar::DevicePicker {
 private:
  const DevicePicker::Button* selectedButton() const override {
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
};

class MockUnzipThread : public UnzipThread {
 public:
  MockUnzipThread(const QFileInfo& inputFile, QObject* parent = 0)
      : UnzipThread(inputFile, parent) {}

  const QString& getFileName() const { return kewlstr; }

  const QString& kewlstr = "";

  void run() override {}
};

class MockDownloadProgressPage : public DownloadProgressPage {
 public:
  MockDownloadProgressPage(DownloadManager* manager_in) {
    manager = manager_in;
    init();
  }

  UnzipThread* makeUnzipThread() {
    return new MockUnzipThread(manager->outputFileInfo(), this);
  }
};

class MockDiskWriteThread : public DiskWriteThread {
 public:
  MockDiskWriteThread(DeviceGuy* drive_in,
                      const QString& image_path_in,
                      QObject* parent = 0)
      : DiskWriteThread(drive_in, image_path_in, parent) {}

  void run() override {}
};

class MockWriteOperationPage : public WriteOperationPage {
 public:
  DiskWriteThread* makeDiskWriteThread(DeviceGuy* drive_in,
                                       const QString& image_path_in,
                                       QObject* parent) override {
    return new MockDiskWriteThread(drive_in, image_path_in, parent);
  }
};

class MockDownloadManager : public DownloadManager {
 public:
  void append(const QUrl& url) override {}
};

class MockNewestImage : public NewestImageUrl {
 public:
  void fetch() override { }
  bool isReady() const override {
    return true;
  } 
};

}  // namespace

// end test object stuff

inline void initResource() {
  Q_INIT_RESOURCE(gondarwizard);
}

namespace gondar {

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
  picker.refresh({DeviceGuy(2, "b", getValidDiskSize()),
                  DeviceGuy(3, "c", getValidDiskSize())});
  QVERIFY(picker.selectedDevice() == nullopt);

  // Select the last device
  auto* btn = getDevicePickerButton(&picker, 1);
  btn->click();

  QCOMPARE(*picker.selectedDevice(), DeviceGuy(3, "c", getValidDiskSize()));
}

void proceed(GondarWizard* wizard) {
  // occasionally with 3 seconds (3k ms) i get stuck on page 3.
  QTest::mouseClick(wizard->button(QWizard::NextButton), Qt::LeftButton,
                    Qt::NoModifier, QPoint(), 500);
  LOG_WARNING << "id after click=" << wizard->currentId();
}

void Test::testLinuxStubFlow() {
  initResource();
  gondar::InitializeLogging();
  MockDevicePicker* testpicker = new MockDevicePicker();
  MockDownloadManager* testmgr = new MockDownloadManager();
  DownloadProgressPage* testprogress = new MockDownloadProgressPage(testmgr);
  WriteOperationPage* testWriteOp = new MockWriteOperationPage();
  NewestImageUrl* testNewest = new MockNewestImage();
  ImageSelectPage* testImageSelect = new ImageSelectPage(testNewest);
  GondarWizard wizard(testpicker, testprogress, testWriteOp, testImageSelect);
  wizard.show();
  QTest::qWait(1000);
  // 0->3
  proceed(&wizard);
  // 3->4
  proceed(&wizard);
  // 4->5
  proceed(&wizard);
  // 5->6
  proceed(&wizard);
  // wait a sec'
  QTest::qWait(1000);
  QVERIFY(wizard.currentId() == 6);
}

}  // namespace gondar

QTEST_MAIN(gondar::Test)
