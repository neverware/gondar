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

#include "gondarwizard.h"

#include <QTimer>

#include "about_dialog.h"
#include "admin_check_page.h"
#include "device_select_page.h"
#include "error_page.h"
#include "log.h"
#include "metric.h"
#include "site_select_page.h"

class GondarWizard::Private {
 public:
  gondar::AboutDialog aboutDialog;

  AdminCheckPage adminCheckPage;
  DeviceSelectPage deviceSelectPage;
  SiteSelectPage siteSelectPage;
  ErrorPage errorPage;

  QDateTime runTime;
};

GondarWizard::GondarWizard(QWidget* parent)
    : QWizard(parent),
      p_(new Private()),
      about_shortcut_(QKeySequence::HelpContents, this) {
  // these pages are automatically cleaned up
  // new instances are made whenever navigation moves on to another page
  // according to qt docs
  setPage(Page_adminCheck, &p_->adminCheckPage);
  // chromeoverLogin and imageSelect are alternatives to each other
  // that both progress to usbInsertPage
  setPage(Page_chromeoverLogin, &chromeoverLoginPage);
  setPage(Page_siteSelect, &p_->siteSelectPage);
  setPage(Page_imageSelect, &imageSelectPage);
  setPage(Page_usbInsert, &usbInsertPage);
  setPage(Page_deviceSelect, &p_->deviceSelectPage);
  setPage(Page_downloadProgress, &downloadProgressPage);
  setPage(Page_writeOperation, &writeOperationPage);
  setPage(Page_error, &p_->errorPage);
  setWizardStyle(QWizard::ModernStyle);
  setWindowTitle(tr("CloudReady USB Maker"));
  setPixmap(QWizard::LogoPixmap, QPixmap(":/images/crlogo.png"));

  setButtonText(QWizard::CustomButton1, "Make Another USB");
  setButtonText(QWizard::CustomButton2, "About");
  setNormalLayout();
  // remove '?' button that does not do anything in our current setup
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  connect(&about_shortcut_, &QShortcut::activated, &p_->aboutDialog,
          &gondar::AboutDialog::show);
  connect(this, SIGNAL(customButtonClicked(int)), this,
          SLOT(handleCustomButton(int)));

  p_->runTime = QDateTime::currentDateTime();
}

GondarWizard::~GondarWizard() {}

void GondarWizard::setNormalLayout() {
  QList<QWizard::WizardButton> button_layout;
  button_layout << QWizard::CustomButton2 << QWizard::Stretch
                << QWizard::NextButton << QWizard::FinishButton;
  setButtonLayout(button_layout);
}

void GondarWizard::setMakeAnotherLayout() {
  QList<QWizard::WizardButton> button_layout;
  button_layout << QWizard::CustomButton2 << QWizard::Stretch
                << QWizard::CustomButton1 << QWizard::NextButton
                << QWizard::FinishButton;
  setButtonLayout(button_layout);
}
// handle event when 'make another usb' button pressed
void GondarWizard::handleCustomButton(int buttonIndex) {
  if (buttonIndex == QWizard::CustomButton1) {
    setNormalLayout();
    // works as long as usbInsertPage is not the last page in wizard
    setStartId(usbInsertPage.nextId() - 1);
    restart();
  } else if (buttonIndex == QWizard::CustomButton2) {
    p_->aboutDialog.show();
  } else {
    LOG_ERROR << "Unknown custom button pressed";
  }
}

int GondarWizard::nextId() const {
  if (p_->errorPage.errorEmpty()) {
    return QWizard::nextId();
  } else {
    if (currentId() == Page_error) {
      return -1;
    } else {
      return Page_error;
    }
  }
}

void GondarWizard::postError(const QString& error) {
  QTimer::singleShot(0, this, [=]() { catchError(error); });
}

void GondarWizard::catchError(const QString& error) {
  LOG_ERROR << "displaying error: " << error;
  p_->errorPage.setErrorString(error);
  // TODO: sanitize error string?
  gondar::SendMetric(gondar::Metric::Error, error.toStdString());
  next();
}

qint64 GondarWizard::getRunTime() {
  return p_->runTime.secsTo(QDateTime::currentDateTime());
}
