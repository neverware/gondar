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

#ifndef SRC_WRITE_OPERATION_PAGE_H_
#define SRC_WRITE_OPERATION_PAGE_H_

#include <QProgressBar>
#include <QVBoxLayout>

#include "device.h"
#include "wizard_page.h"

class DiskWriteThread;

class WriteOperationPage : public gondar::WizardPage {
  Q_OBJECT

 public:
  WriteOperationPage(QWidget* parent = 0);

  void setDevice(const DeviceGuy& device);

 protected:
  void initializePage() override;
  bool isComplete() const override;
  bool validatePage() override;
  void showProgress();
  int nextId() const override;
  virtual DiskWriteThread* makeDiskWriteThread(DeviceGuy* drive_in,
                                               const QString& image_path_in,
                                               QObject* parent);
 public slots:
  void onDoneWriting();

 private:
  void writeToDrive();
  void writeFailed(const QString& errorMessage);
  QVBoxLayout layout;
  QProgressBar progress;
  bool writeFinished;
  DiskWriteThread* diskWriteThread;
  QString image_path;
  DeviceGuy device;
};

#endif  // SRC_WRITE_OPERATION_PAGE_H_
