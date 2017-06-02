
#include <QApplication>
#include <QLibraryInfo>

#include "gondarwizard.h"

#include <QtPlugin>

int main(int argc, char *argv[])
{
    Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
    QApplication app(argc, argv);
    GondarWizard wizard;
    wizard.show();
    return app.exec();
}
