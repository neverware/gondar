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

#include <QApplication>
#include <QLibraryInfo>
#include <QtPlugin>

#if defined(Q_OS_WIN)
#include "dismissprompt.h"
#endif
#include "gondar.h"
#include "gondarwizard.h"
#include "log.h"
#include "metric.h"
#include "util.h"

int main(int argc, char* argv[]) {
#if defined(Q_OS_WIN)
  Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif
  Q_INIT_RESOURCE(gondarwizard);
  gondar::InitializeLogging();
  gondar::SendMetric(gondar::Metric::Use);
#if defined(Q_OS_WIN)
  // dismiss Windows 'format disk' popups
  // placed here to enable logging for this step
  SetFormatPromptHook();
#endif
  dontcompile
  QApplication app(argc, argv);
  app.setStyleSheet(gondar::readUtf8File(":/style.css"));

  GondarWizard wizard;
  wizard.show();

  const auto ret = app.exec();
  LOG_INFO << "app.exec() returned " << ret;
  CleanUp();
  return ret;
}
