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

#include "image_select_page.h"

#include "gondarwizard.h"
#include "log.h"
#include "util.h"

ImageSelectPage::ImageSelectPage(QWidget* parent) : WizardPage(parent) {
  setTitle("Which version of CloudReady do you need?");
  setSubTitle("Choose between 32-bit and 64-bit installers");

  sixtyFourDetails.setText("Suitable for most computers made after 2007");
  thirtyTwoDetails.setText(
      "For older computers or devices with Intel Atom CPUs");

  thirtyTwo.setText("32-bit");
  sixtyFour.setText("64-bit (recommended)");
  sixtyFour.setChecked(true);
  bitnessButtons.addButton(&thirtyTwo);
  bitnessButtons.addButton(&sixtyFour);

  layout.addWidget(&sixtyFour);
  layout.addWidget(&sixtyFourDetails);
  layout.addWidget(&thirtyTwo);
  layout.addWidget(&thirtyTwoDetails);

  setLayout(&layout);
  connect(&newestImageUrl, &NewestImageUrl::errorOccurred, this,
          &ImageSelectPage::handleNewestImageUrlError);
  hasError = false;
}

void ImageSelectPage::initializePage() {
  if (!gondar::isChromeover()) {
    // for beerover, we'll have to check what the latest release is
    newestImageUrl.fetch();
  }
}

bool ImageSelectPage::validatePage() {
  // we only need to prevent proceeding to next page in the beerover case
  if (gondar::isChromeover()) {
    return true;
    // if there is an error, we need to allow the user to proceed to the error
    // screen
  } else if (hasError) {
    return true;
  } else {
    return newestImageUrl.isReady();
  }
}

int ImageSelectPage::nextId() const {
  return GondarWizard::Page_usbInsert;
}

void ImageSelectPage::set32Url(QUrl url_in) {
  newestImageUrl.set32Url(url_in);
}

void ImageSelectPage::set64Url(QUrl url_in) {
  newestImageUrl.set64Url(url_in);
}

QUrl ImageSelectPage::getUrl() {
  QAbstractButton* selected = bitnessButtons.checkedButton();
  if (selected == &thirtyTwo) {
    return newestImageUrl.get32Url();
  } else if (selected == &sixtyFour) {
    return newestImageUrl.get64Url();
  } else {
    // TODO: decide what this behavior should be
    return newestImageUrl.get64Url();
  }
}

void ImageSelectPage::handleNewestImageUrlError() {
  hasError = true;
  wizard()->postError("No network connection detected. Please connect to a network in order to retrieve the site image.");
}
