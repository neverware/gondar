#include "metric.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

#include "config.h"
#include "log.h"

namespace gondar {

namespace {

QNetworkAccessManager* getNetworkManager() {
  static QNetworkAccessManager manager;
  return &manager;
}

std::string getMetricString(Metric metric) {
  switch (metric) {
    case Metric::BeeroverUse:
      return "beerover-use";
    case Metric::ChromeoverUse:
      return "chromeover-use";
    case Metric::DownloadAttempt:
      return "download-attempt";
    case Metric::DownloadSuccess:
      return "download-success";
    case Metric::UsbAttempt:
      return "usb-attempt";
    case Metric::UsbSuccess:
      return "usb-success";
    case Metric::Use:
      return "use";
    // not sure we want to crash the program on a bad metric lookup
    default:
      return "unknown";
  }
}

QByteArray getMetricsApiKey() {
#ifdef METRICS_API_KEY
  return QByteArray(METRICS_API_KEY);
#else
  return QByteArray();
#endif
}
}

void SendMetric(Metric metric) {
  const auto api_key = getMetricsApiKey();
  if (api_key.isEmpty()) {
    // all production builds should sent metrics
    LOG_WARNING << "not sending metrics!";
    return;
  }
  std::string metricStr = getMetricString(metric);
  QNetworkAccessManager* manager = getNetworkManager();
  QUrl url("https://gondar-metrics.neverware.com/prod");
  QJsonObject json;
  // TODO: use a persistent UUID across a session, and potentially even
  // across multiple runs
  QString id = QUuid::createUuid().toString();
  json["identifier"] = id;
  json.insert("metric", QString::fromStdString(metricStr));
  QNetworkRequest request(url);
  request.setRawHeader(QByteArray("x-api-key"), api_key);
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    "application/x-www-form-urlencoded");
  QJsonDocument doc(json);
  QString strJson(doc.toJson(QJsonDocument::Compact));
  manager->post(request, QByteArray(strJson.toUtf8()));
}
}
