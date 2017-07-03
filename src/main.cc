
#include <QApplication>
#include <QLibraryInfo>
#include <QtPlugin>

#include "gondarwizard.h"
#include "plog/Log.h"

int main(int argc, char* argv[]) {
#if defined(Q_OS_WIN)
  Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif
  Q_INIT_RESOURCE(gondarwizard);

  plog::init(plog::debug, "log1.txt");

  QApplication app(argc, argv);
  GondarWizard wizard;
  wizard.show();
  return app.exec();
}
