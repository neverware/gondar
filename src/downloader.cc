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

#include "downloader.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "log.h"
#include "metric.h"

DownloadManager::DownloadManager(QObject* parent)
    : QObject(parent), downloadedCount(0), totalCount(0) {}

void DownloadManager::append(const QStringList& urlList) {
  foreach (QString url, urlList)
    append(QUrl::fromEncoded(url.toLocal8Bit()));

  if (downloadQueue.isEmpty())
    QTimer::singleShot(0, this, &DownloadManager::finished);
}

void DownloadManager::append(const QUrl& url) {
  if (downloadQueue.isEmpty())
    QTimer::singleShot(0, this, &DownloadManager::startNextDownload);

  downloadQueue.enqueue(url);
  ++totalCount;
}

QString DownloadManager::saveFileName(const QUrl& url) {
  return url.fileName();
}

void DownloadManager::startNextDownload() {
  if (downloadQueue.isEmpty()) {
    LOG_INFO << downloadedCount << "/" << totalCount
             << " files downloaded successfully";
    emit finished();
    return;
  }

  QUrl url = downloadQueue.dequeue();

  const QDir dir =
      QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
  QString filename = saveFileName(url);

  output.setFileName(dir.filePath(filename));
  qInfo() << "Download destination:" << output.fileName();

  if (!output.open(QIODevice::WriteOnly)) {
    LOG_ERROR << "failed to open " << filename << ": " << output.errorString();
    LOG_ERROR << "skipping download of " << url;
    startNextDownload();
    return;  // skip this download
  }

  QNetworkRequest request(url);
  gondar::SendMetric(gondar::Metric::DownloadAttempt);
  currentDownload = manager.get(request);
  connect(currentDownload, &QNetworkReply::finished, this,
          &DownloadManager::downloadFinished);
  connect(currentDownload, &QNetworkReply::readyRead, this,
          &DownloadManager::downloadReadyRead);
  emit started();

  // prepare the output
  LOG_INFO << "downloading " << url;
  downloadTime.start();
}

void DownloadManager::downloadFinished() {
  gondar::SendMetric(gondar::Metric::DownloadSuccess);
  // progressBar.clear();
  output.close();

  if (currentDownload->error()) {
    // download failed
    LOG_ERROR << "download failed: " << currentDownload->errorString();
  } else {
    LOG_INFO << "download succeeded";
    ++downloadedCount;
  }

  currentDownload->deleteLater();
  startNextDownload();
}

void DownloadManager::downloadReadyRead() {
  output.write(currentDownload->readAll());
}

QNetworkReply* DownloadManager::getCurrentDownload() {
  return currentDownload;
}

QFileInfo DownloadManager::outputFileInfo() const {
  LOG_WARNING << "~~~RETURNING " << output.fileName();
  return QFileInfo(output.fileName());
}
