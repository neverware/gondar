#include "log.h"

#include <cstdio>

#include <QDir>
#include <QStandardPaths>

#include <plog/Appenders/ConsoleAppender.h>

namespace gondar {

namespace {

// Get the log path, creating the parent directory if necessary.
std::string CreateLogPath() {
  const auto vendor = QStringLiteral("neverware");
  const auto product = QStringLiteral("cloudready-usb-creator.log");

  const QDir dir =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

  // Fail if the app data location doesn't exist
  if (!dir.exists()) {
    fprintf(stderr, "directory does not exist: %s\n",
            dir.path().toStdString().c_str());
    return std::string();
  }

  // Create our vendor subdirectory if it doesn't already exist
  const QDir subdir = dir.filePath(vendor);
  if (!subdir.exists() && !dir.mkdir(vendor)) {
    fprintf(stderr, "failed to create directory: %s\n",
            subdir.path().toStdString().c_str());
    return std::string();
  }

  const auto path = subdir.filePath(product);
  return path.toStdString();
}

}  // namespace

void InitializeLogging() {
  const plog::Severity max_severity = plog::debug;
  const std::string path = CreateLogPath();
  const size_t max_file_size = 5 * 1024 * 1024;  // 5 MiB
  const int max_files = 2;

  fprintf(stderr, "initializing log: %s\n", path.c_str());

  // Log to both log files and to the console (if present)
  static plog::RollingFileAppender<plog::TxtFormatter> file_log(
      path.c_str(), max_file_size, max_files);
  static plog::ConsoleAppender<plog::TxtFormatter> console_log;

  plog::init(max_severity, &file_log).addAppender(&console_log);

  LOG_INFO << "log initialized";
}

}  // namespace gondar
