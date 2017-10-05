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

#include "write_operation_page.h"

#include "diskwritethread.h"
#include "gondarwizard.h"
#include "log.h"
#include "metric.h"

WriteOperationPage::WriteOperationPage(QWidget* parent)
    : WizardPage(parent), device(0, std::string(), 0) {
  layout.addWidget(&progress);
  setLayout(&layout);
}

void WriteOperationPage::setDevice(const DeviceGuy& device_in) {
  device = device_in;
}

void WriteOperationPage::initializePage() {
  // set the titles in initializePage for 'make another' flow
  setTitle("Creating your CloudReady USB installer");
  setSubTitle("This process may take up to 20 minutes.");
  writeFinished = false;
  writeToDrive();
}

bool WriteOperationPage::isComplete() const {
  return writeFinished;
}

bool WriteOperationPage::validatePage() {
  return writeFinished;
}

DiskWriteThread* WriteOperationPage::makeDiskWriteThread(
    DeviceGuy* drive_in,
    const QString& image_path_in,
    QObject* parent) {
  return new DiskWriteThread(drive_in, image_path_in, parent);
}

void WriteOperationPage::writeToDrive() {
  LOG_INFO << "Writing to drive...";
  image_path.clear();
  image_path.append(wizard()->downloadProgressPage->getImageFileName());
  showProgress();
  diskWriteThread = makeDiskWriteThread(&device, image_path, this);
  connect(diskWriteThread, &DiskWriteThread::finished, this,
          &WriteOperationPage::onDoneWriting);
  LOG_INFO << "launching thread...";
  gondar::SendMetric(gondar::Metric::UsbAttempt);
  diskWriteThread->start();
}

void WriteOperationPage::showProgress() {
  progress.setRange(0, 0);
  progress.setValue(0);
}

void WriteOperationPage::onDoneWriting() {
  switch (diskWriteThread->state()) {
    case DiskWriteThread::State::Initial:
    case DiskWriteThread::State::Running:
      // It should not be possible to get here at runtime
      writeFailed("Internal state error");
      return;

    case DiskWriteThread::State::GetFileSizeFailed:
      writeFailed("Error reading the disk image's file size");
      return;

    case DiskWriteThread::State::InstallFailed:
      writeFailed("Error writing to the USB device");
      return;

    case DiskWriteThread::State::Success:
      // Hooray!
      gondar::SendMetric(gondar::Metric::UsbSuccess);
      break;
  }

  setTitle("CloudReady USB created!");
  setSubTitle("You may now either exit or create another USB.");
  qDebug() << "install call returned";
  writeFinished = true;
  progress.setRange(0, 100);
  progress.setValue(100);
  wizard()->setMakeAnotherLayout();
  // when a USB was successfully created, report time the run took
  gondar::SendMetric(gondar::Metric::SuccessDuration,
                     QString::number(wizard()->getRunTime()).toStdString());
  emit completeChanged();
}

// though error page follows in index, this is the end of the wizard for
// healthy flows
int WriteOperationPage::nextId() const {
  return -1;
}

void WriteOperationPage::writeFailed(const QString& errorMessage) {
  wizard()->postError(errorMessage);
  writeFinished = true;
  emit completeChanged();
}
