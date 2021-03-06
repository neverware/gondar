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

#include "meepo.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

#include "config.h"
#include "gondarsite.h"
#include "log.h"
#include "metric.h"
#include "util.h"

namespace {

const char path_activity[] = "/activity";
const char path_auth[] = "/auth";
const char path_google_auth[] = "/google-auth";
const char path_sites[] = "/sites";
const char path_downloads[] = "/downloads";

QUrl createUrl(const QString& path) {
  return QUrl("https://api." + gondar::getDomain() + "/poof" + path);
}

int siteIdFromUrl(const QUrl& url) {
  const auto path = url.path();
  const auto parts = path.split('/');
  const auto sites_index = parts.lastIndexOf("sites");

  if (sites_index == -1) {
    LOG_ERROR << "failed to find 'sites' in " << url.toString();
    return -1;
  }

  const auto site_id_index = sites_index + 1;
  if (site_id_index >= parts.size()) {
    LOG_ERROR << "url ended without a site ID: " << url.toString();
    return -1;
  }

  const auto site_id_str = parts[site_id_index];

  bool ok = false;
  const auto site_id = site_id_str.toInt(&ok);

  if (!ok) {
    LOG_ERROR << "site ID is not an integer: " << url.toString();
    return -1;
  }

  return site_id;
}

QNetworkRequest createAuthRequest() {
  auto url = createUrl(path_auth);
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  return request;
}

QNetworkRequest createGoogleAuthRequest() {
  auto url = createUrl(path_google_auth);
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  return request;
}

QNetworkRequest createSitesRequest(const QString& api_token, int page) {
  auto url = createUrl(path_sites);
  QUrlQuery query;
  query.addQueryItem("token", api_token);
  query.addQueryItem("page", QString::number(page));
  url.setQuery(query);
  return QNetworkRequest(url);
}

QNetworkRequest createDownloadsRequest(const QString& api_token,
                                       const int site_id) {
  const auto path =
      QString("%1/%2%3").arg(path_sites).arg(site_id).arg(path_downloads);
  auto url = createUrl(path);
  QUrlQuery query;
  query.addQueryItem("token", api_token);
  url.setQuery(query);
  QNetworkRequest request(url);
  return request;
}

std::vector<GondarSite> sitesFromReply(const QJsonArray& rawSites) {
  std::vector<GondarSite> sites;

  for (const QJsonValue& cur : rawSites) {
    const QJsonObject site = cur.toObject();
    const auto site_id = site["site_id"].toInt();
    const auto site_name = site["name"].toString();
    sites.emplace_back(GondarSite(site_id, site_name));
  }

  return sites;
}

// Get the next page from the pagination dict. If on the last page, return 0.
int getNextPage(const QJsonObject& outer_json) {
  const QJsonObject json = outer_json["pagination"].toObject();
  auto cur = json.value("current").toInt();
  auto total = json.value("total").toInt();
  if (cur < total) {
    return cur + 1;
  } else {
    return 0;
  }
}

}  // namespace

namespace gondar {

Meepo::Meepo() {
  // All replies are handled by dispatchReply
  connect(&network_manager_, &QNetworkAccessManager::finished, this,
          &Meepo::dispatchReply);
}

void Meepo::clear() {
  api_token_.clear();
  sites_.clear();
  error_.clear();
  sites_remaining_ = 0;
}

void Meepo::start(const QAuthenticator& auth) {
  google_mode_ = false;
  LOG_INFO << "starting meepo flow";
  clear();
  requestAuth(auth);
}

void Meepo::startGoogle(QString id_token) {
  google_mode_ = true;
  LOG_INFO << "starting meepo flow with google";
  clear();
  requestGoogleAuth(id_token);
}

bool Meepo::hasToken() {
  return !api_token_.isEmpty();
}

// tests use this
void Meepo::setToken(QString token_in) {
  api_token_ = token_in;
}

QString Meepo::error() const {
  return error_;
}

Meepo::Sites Meepo::sites() const {
  return sites_;
}

void Meepo::requestAuth(const QAuthenticator& auth) {
  QJsonObject json;
  json["email"] = auth.user();
  json["password"] = auth.password();
  QJsonDocument doc(json);
  auto request = createAuthRequest();
  LOG_INFO << "POST " << request.url().toString();
  network_manager_.post(request, doc.toJson(QJsonDocument::Compact));
}

void Meepo::requestGoogleAuth(QString id_token) {
  QJsonObject json;
  json["id_token"] = id_token;
  LOG_INFO << "id_token =" << id_token;
  QJsonDocument doc(json);
  auto request = createGoogleAuthRequest();
  LOG_INFO << "POST " << request.url().toString();
  network_manager_.post(request, doc.toJson(QJsonDocument::Compact));
}

void Meepo::handleAuthReply(QNetworkReply* reply) {
  api_token_ = jsonFromReply(reply)["api_token"].toString();

  if (api_token_.isEmpty()) {
    fail("invalid API token");
    return;
  }

  LOG_INFO << "token received";
  // request the first page of sites
  requestSites(1);
}

void Meepo::requestSites(int page) {
  const auto request = createSitesRequest(api_token_, page);
  LOG_INFO << "GET " << request.url().toString();
  network_manager_.get(request);
}

void Meepo::handleSitesReply(QNetworkReply* reply) {
  const QJsonObject json = gondar::jsonFromReply(reply);
  const QJsonArray rawSites = json["sites"].toArray();
  auto new_sites = sitesFromReply(rawSites);
  sites_.insert(sites_.end(), new_sites.begin(), new_sites.end());

  LOG_INFO << "received " << sites_.size() << " site(s)";

  // sites starts at zero, and every time we go get another page,
  // there are more sites remaining to get downloads from
  sites_remaining_ += sites_.size();
  LOG_INFO << "sites_remaining increased, now " << sites_remaining_;

  // this logic should still work for the multi-page case
  if (sites_remaining_ == 0) {
    fail(no_sites_error);
    return;
  }

  for (const auto& site : sites_) {
    requestDownloads(site);
  }
  // see if there's another batch
  int next_page = getNextPage(json);
  if (next_page > 0) {
    LOG_INFO << "getting next page of sites, page " << next_page;
    requestSites(next_page);
  }
}

void Meepo::requestDownloads(const GondarSite& site) {
  const auto request = createDownloadsRequest(api_token_, site.getSiteId());
  LOG_INFO << "GET " << request.url().toString();
  network_manager_.get(request);
}

void Meepo::handleDownloadsReply(QNetworkReply* reply) {
  const auto site_id_from_reply = siteIdFromUrl(reply->url());
  if (site_id_from_reply == -1) {
    fail("missing site ID");
    return;
  }

  GondarSite* site = siteFromSiteId(site_id_from_reply);
  if (!site) {
    fail("site not found");
    return;
  }

  const auto jsonObj = jsonFromReply(reply);
  QJsonObject productsObj = jsonObj["links"].toObject();

  for (const auto& product : productsObj.keys()) {
    for (const auto& image : productsObj[product].toArray()) {
      const auto imageObj = image.toObject();
      const auto imageName(imageObj["title"].toString());
      const QUrl url(imageObj["url"].toString());
      LOG_INFO << "Product: " << product << ", Image name:" << imageName;
      GondarImage gondarImage(product, imageName, url);
      site->addImage(gondarImage);
    }
  }
  sites_remaining_--;
  LOG_INFO << "processed a site; sites remaining = " << sites_remaining_;

  // see if we're done
  if (sites_remaining_ == 0) {
    LOG_INFO << "received information for all outstanding site requests";
    // we don't want users to be able to pass through the screen by pressing
    // next while processing.  this will make validatePage pass and immediately
    // move the user on to the next screen
    emit finished();
  }
}

QString Meepo::getMetricJson(std::string metric, std::string value) {
  QJsonObject json;
  QJsonObject inner_json;
  QString activity_string =
      QString("usb-maker-%1").arg(QString::fromStdString(metric));
  inner_json.insert("activity", activity_string);
  // meepo already knows the site.
  // description should become a combination of gondar version and value if it
  // exists
  QString none_string = QString("none");
  QString version_string = gondar::getGondarVersion();
  if (version_string.length() == 0) {
    version_string = none_string;
  }
  QString value_string = QString::fromStdString(value);
  if (value_string.length() == 0) {
    value_string = none_string;
  }
  QString description_string =
      QString("version=%1,value=%2").arg(version_string).arg(value_string);
  inner_json.insert("description", description_string);
  // removed explicit isChromeover() check here for tests
  // because we already only send on non-zero site id, it was somewhat
  // redundant to also check for chromeover.  also currently beerover
  // has no meepo interaction so meaning of this case was not clear
  if (site_id != 0) {
    inner_json.insert("site_id", site_id);
  }
  json["activity"] = inner_json;
  QJsonDocument doc(json);
  QString strJson(doc.toJson(QJsonDocument::Compact));
  return doc.toJson(QJsonDocument::Compact);
}

QNetworkRequest Meepo::getMetricRequest() {
  auto url = createUrl(path_activity);
  QUrlQuery query;
  query.addQueryItem("token", api_token_);
  url.setQuery(query);
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  return request;
}

void Meepo::sendMetric(std::string metric, std::string value) {
  QNetworkRequest request = getMetricRequest();
  QString json = getMetricJson(metric, value);
  network_manager_.post(request, QByteArray(json.toUtf8()));
}

void Meepo::handleMetricsReply(QNetworkReply* reply) {
  if (reply->error() == QNetworkReply::NoError) {
    LOG_INFO << "Successfully sent meepo metric";
  } else {
    LOG_WARNING << "Received error response sending meepo metric";
  }
}

void Meepo::dispatchReply(QNetworkReply* reply) {
  const auto error = reply->error();
  const auto url = reply->url();

  if (error != QNetworkReply::NoError) {
    // TODO(nicholasbishop): make this more readable
    LOG_ERROR << "network error: " << url.toString() << std::endl
              << ", error " << error;
    // TODO(nicholasbishop): move the error handling into each of the
    // three handlers below so that errors can be more specific
    fail("network error");
    // FIXME: use the constants at top of file instead
  } else if (url.path().endsWith("/auth")) {
    handleAuthReply(reply);
  } else if (url.path().endsWith("/google-auth")) {
    handleAuthReply(reply);
  } else if (url.path().endsWith("/sites")) {
    handleSitesReply(reply);
  } else if (url.path().endsWith("/downloads")) {
    handleDownloadsReply(reply);
  } else if (url.path().endsWith("/activity")) {
    handleMetricsReply(reply);
  }
  reply->deleteLater();
}

void Meepo::fail(const QString& error) {
  LOG_ERROR << "error: " << error;
  error_ = error;
  emit failed(google_mode_);
}

GondarSite* Meepo::siteFromSiteId(const int site_id_in) {
  for (auto& site : sites_) {
    if (site.getSiteId() == site_id_in) {
      return &site;
    }
  }
  return nullptr;
}

int Meepo::getSiteId() {
  return site_id;
}

void Meepo::setSiteId(int site_id_in) {
  site_id = site_id_in;
}

}  // namespace gondar
