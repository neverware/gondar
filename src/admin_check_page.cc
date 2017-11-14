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

#include "admin_check_page.h"
#include "gondar.h"
#include "gondarwizard.h"
#include "metric.h"
#include "util.h"

// note that even though this is called the admin check page, it will in most
// cases be a welcome page, unless the user is missing admin rights
AdminCheckPage::AdminCheckPage(QWidget* parent) : WizardPage(parent) {
  is_admin = false;  // assume false until we discover otherwise.
                     // this holds the user at this screen

  layout.addWidget(&label);
  layout.addStretch();
  layout.addWidget(&warpTunnel);
  setLayout(&layout);
  // TODO: move isChromevoer() out of AdminCheckPage and call these metrics
  // from a more intuitive context
  if (gondar::isChromeover()) {
    gondar::SendMetric(gondar::Metric::ChromeoverUse);
  } else {
    gondar::SendMetric(gondar::Metric::BeeroverUse);
  }
}

void AdminCheckPage::handleFormatOnly() {
  wizard()->setFormatOnly(true);
  wizard()->next();
}

void AdminCheckPage::initializePage() {
  is_admin = IsCurrentProcessElevated();
  if (!is_admin) {
    showIsNotAdmin();
  } else {
    showIsAdmin();
  }
}

bool AdminCheckPage::isComplete() const {
  // the next button is grayed out if user does not have appropriate rights
  return is_admin;
}

void AdminCheckPage::showIsAdmin() {
  setTitle("Welcome to the CloudReady USB Creation Utility");
  // note that a subtitle must be set  on a page in order for logo to display
  setSubTitle(
      "This utility will create a USB device that can be used to install "
      "CloudReady on any computer.");
  label.setText(
      "<p>You will need:</p><ul><li>8GB or 16GB USB stick</li><li>20 minutes "
      "for USB installer creation</li></ul></p>");
  label.setWordWrap(true);
  warpTunnel.setText(
      "<a href=\"reformat\">Done with your USB installer?  Format it "
      "here.</a>");
  warpTunnel.setTextFormat(Qt::RichText);
  connect(&warpTunnel, &QLabel::linkActivated, this,
          &AdminCheckPage::handleFormatOnly);
  emit completeChanged();
}

void AdminCheckPage::showIsNotAdmin() {
  setTitle("User does not have administrator rights");
  setSubTitle(
      "The current user does not have adminstrator rights or the program was "
      "run without sufficient rights to create a USB.  Please re-run the "
      "program with sufficient rights.");
}

int AdminCheckPage::nextId() const {
  if (wizard()->isFormatOnly()) {
    return GondarWizard::Page_usbInsert;
  }
  if (gondar::isChromeover()) {
    return GondarWizard::Page_chromeoverLogin;
  } else {
    return GondarWizard::Page_imageSelect;
  }
}
