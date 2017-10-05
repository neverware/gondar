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

#include "download_progress_page.h"
#include "gondarwizard.h"

DownloadProgressPage::DownloadProgressPage(QWidget* parent)
    : WizardPage(parent) {
  setTitle("CloudReady Download");
  setSubTitle("Your installer image is currently downloading.");
  download_finished = false;
  layout.addWidget(&progress);
  setLayout(&layout);
  range_set = false;
}

void DownloadProgressPage::initializePage() {
  setLayout(&layout);
  const QUrl url = wizard()->imageSelectPage.getUrl();
  qDebug() << "using url= " << url;
  connect(&manager, &DownloadManager::finished, this,
          &DownloadProgressPage::markComplete);
  manager.append(url.toString());
  connect(&manager, &DownloadManager::started, this,
          &DownloadProgressPage::onDownloadStarted);
}

void DownloadProgressPage::onDownloadStarted() {
  QNetworkReply* cur_download = manager.getCurrentDownload();
  connect(cur_download, &QNetworkReply::downloadProgress, this,
          &DownloadProgressPage::downloadProgress);
}

void DownloadProgressPage::downloadProgress(qint64 sofar, qint64 total) {
  if (!range_set) {
    range_set = true;
    progress.setRange(0, total);
  }
  progress.setValue(sofar);
}

void DownloadProgressPage::markComplete() {
  download_finished = true;
  // now that the download is finished, let's unzip the build.
  notifyUnzip();
  // TODO: maybe subclass the network manager to supply our stub file located
  // at the path we give for outputFileInfo()
  unzipThread = new UnzipThread(manager.outputFileInfo(), this);
  connect(unzipThread, &UnzipThread::finished, this,
          &DownloadProgressPage::onUnzipFinished);
  unzipThread->start();
}

void DownloadProgressPage::onUnzipFinished() {
  // unzip has now completed
  qDebug() << "main thread has accepted complete";
  progress.setRange(0, 100);
  progress.setValue(100);
  setSubTitle("Download and extraction complete!");
  emit completeChanged();
  // immediately progress to writeOperationPage
  wizard()->next();
}
void DownloadProgressPage::notifyUnzip() {
  setSubTitle("Extracting compressed image...");
  // setting range and value to zero results in an 'infinite' progress bar
  progress.setRange(0, 0);
  progress.setValue(0);
}

bool DownloadProgressPage::isComplete() const {
  return download_finished;
}

const QString& DownloadProgressPage::getImageFileName() {
  return unzipThread->getFileName();
}
