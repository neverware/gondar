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

#ifndef UNZIP_THREAD_H
#define UNZIP_THREAD_H

#include <QFileInfo>
#include <QThread>

class UnzipThread : public QThread {
  Q_OBJECT
 public:
  UnzipThread(const QFileInfo& inputFile, QObject* parent = 0);
  ~UnzipThread();
  virtual const QString& getFileName() const;

 protected:
  virtual void run() override;
  QFileInfo inputFile;
  QString filename;
};

#endif /* UNZIP_THREAD_H */
