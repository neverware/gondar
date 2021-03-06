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

#include "util.h"

#include <QFile>
#include <QJsonDocument>
#include <QNetworkReply>
#include <random>

#include "config.h"
#include "log.h"

namespace gondar {

// get the google sign in client id
// see https://console.developers.google.com
QByteArray getGoogleSignInId() {
#ifdef GOOGLE_SIGN_IN_CLIENT
  return QByteArray(GOOGLE_SIGN_IN_CLIENT);
#else
  return QByteArray();
#endif
}

// get the google sign in client secret
QByteArray getGoogleSignInSecret() {
#ifdef GOOGLE_SIGN_IN_SECRET
  return QByteArray(GOOGLE_SIGN_IN_SECRET);
#else
  return QByteArray();
#endif
}

QString readUtf8File(const QString& filepath) {
  LOG_INFO << "opening " << filepath << " for reading";
  QFile file(filepath);
  if (!file.open(QFile::ReadOnly)) {
    throw std::runtime_error("error opening file for reading: " +
                             filepath.toStdString());
  }
  return QString::fromUtf8(file.readAll());
}

// helper function to determine if this build is a chromeover build
bool isChromeover() {
#ifdef CHROMEOVER
  return true;
#else
  return false;
#endif
}

bool isRelease() {
#ifdef RELEASE
  return true;
#else
  return false;
#endif
}

QString getDomain() {
  if (gondar::isRelease()) {
    LOG_INFO << "Using release licensing endpoint";
    return QString("neverware.com");
  } else {
    LOG_INFO << "Using dev licensing endpoint";
    return QString("grv.neverware.com");
  }
}

QString getGondarVersion() {
  return QString("");
}

QJsonObject jsonFromReply(QNetworkReply* reply) {
  QJsonDocument jsonDoc = QJsonDocument::fromJson(reply->readAll());
  return jsonDoc.object();
}

// the number of bytes in a gigabyte (2**30)
uint64_t getGigabyte() {
  return 1073741824LL;
}
}  // namespace gondar
