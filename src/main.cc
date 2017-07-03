
#include <QApplication>
#include <QLibraryInfo>
#include <QtPlugin>

#include "gondarwizard.h"
#include "log.h"

int main(int argc, char* argv[]) {
#if defined(Q_OS_WIN)
  Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif
  Q_INIT_RESOURCE(gondarwizard);

  gondar::InitializeLogging();

  QApplication app(argc, argv);
  GondarWizard wizard;
  wizard.show();

  const auto ret = app.exec();
  LOG_INFO << "app.exec() returned " << ret;

  return ret;
}
