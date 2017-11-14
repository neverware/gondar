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

#ifndef DISKWRITE_THREAD_H
#define DISKWRITE_THREAD_H

#include <QMutex>
#include <QString>
#include <QThread>

#include "device.h"

class DiskWriteThread : public QThread {
  Q_OBJECT
 public:
  // a constructor used to reformat the disk.  it requires no image path string
  DiskWriteThread(DeviceGuy* drive_in, QObject* parent = 0);
  // a constructor used to write the specified image to disk
  DiskWriteThread(DeviceGuy* drive_in,
                  const QString& image_path_in,
                  QObject* parent = 0);
  ~DiskWriteThread();

  enum class State {
    Initial,
    Running,
    GetFileSizeFailed,
    InstallFailed,
    Success,
  };

  State state() const;

 protected:
  void run() override;

 private:
  void setState(State state);
  void writeImage();
  void formatDrive();

  mutable QMutex state_mutex_;
  State state_ = State::Initial;
  DeviceGuy selected_drive;
  QString image_path;
};

#endif /* DISKWRITE_THREAD_H */
