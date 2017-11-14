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

#include "diskwritethread.h"

#include <QFile>

#include "device.h"
#include "gondar.h"
#include "log.h"
#include "metric.h"

static int64_t getFileSize(QString& path) {
  QFile file(path);
  if (!file.exists()) {
    return -1;
  }
  return file.size();
}

DiskWriteThread::DiskWriteThread(DeviceGuy* drive_in, QObject* parent)
    : QThread(parent), selected_drive(*drive_in) {
  image_path = nullptr;
}

DiskWriteThread::DiskWriteThread(DeviceGuy* drive_in,
                                 const QString& image_path_in,
                                 QObject* parent)
    : QThread(parent), selected_drive(*drive_in) {
  image_path = image_path_in;
}

DiskWriteThread::~DiskWriteThread() {}

DiskWriteThread::State DiskWriteThread::state() const {
  QMutexLocker locker(&state_mutex_);
  return state_;
}

void DiskWriteThread::writeImage() {
  LOG_INFO << "writing " << image_path << " to disk";
  setState(State::Running);

  const int64_t image_size = getFileSize(image_path);
  if (image_size == -1) {
    LOG_ERROR << "getFileSize failed";
    setState(State::GetFileSizeFailed);
    return;
  }

  if (!Install(&selected_drive, image_path.toStdString().c_str(), image_size)) {
    LOG_ERROR << "Install failed";
    setState(State::InstallFailed);
    return;
  }

  LOG_INFO << "Install succeeded";
  setState(State::Success);
}

void DiskWriteThread::formatDrive() {
  LOG_INFO << "formatting disk";
  setState(State::Running);
  if (!Format(&selected_drive)) {
    LOG_ERROR << "Install failed";
    setState(State::InstallFailed);
    return;
  }

  LOG_INFO << "Format succeeded";
  setState(State::Success);
}
void DiskWriteThread::run() {
  if (image_path == nullptr) {
    formatDrive();
  } else {
    writeImage();
  }
}

void DiskWriteThread::setState(const State state) {
  QMutexLocker locker(&state_mutex_);
  state_ = state;
}
