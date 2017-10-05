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

#include "gondarimage.h"

#include "gondarwizard.h"
#include "log.h"
#include "util.h"

class DownloadButton : public QRadioButton {
 public:
  DownloadButton(QUrl& url_in) : url(url_in) {}
  QUrl getUrl() const { return url; }

 private:
  QUrl url;
};

ImageSelectPage::ImageSelectPage(QWidget* parent) : WizardPage(parent) {
  newestImageUrl.reset(new NewestImageUrl());
  init();
}

ImageSelectPage::ImageSelectPage(NewestImageUrl * newIn, QWidget* parent)
    : WizardPage(parent) {
  newestImageUrl.reset(newIn);
  init();
}

void ImageSelectPage::init() {
  setTitle("Which version of CloudReady do you need?");
  setSubTitle(" ");

  thirtyTwo.setText("32-bit");
  thirtyTwoDetails.setText(
      "<a href=\"https://guide.neverware.com/supported-devices\">Only intended "
      "for certified models marked '32-bit Only'</a>");
  thirtyTwoDetails.setTextFormat(Qt::RichText);
  thirtyTwoDetails.setTextInteractionFlags(Qt::TextBrowserInteraction);
  thirtyTwoDetails.setOpenExternalLinks(true);
  // we use these buttons for beerover only now
  if (!gondar::isChromeover()) {
    sixtyFourDetails.setText("Suitable for most computers made after 2007");
    sixtyFour.setText("64-bit (recommended)");
    sixtyFour.setChecked(true);
    bitnessButtons.addButton(&thirtyTwo);
    bitnessButtons.addButton(&sixtyFour);
    layout.addWidget(&sixtyFour);
    layout.addWidget(&sixtyFourDetails);
    layout.addWidget(&thirtyTwo);
    layout.addWidget(&thirtyTwoDetails);
  }

  setLayout(&layout);
  connect(newestImageUrl.get(), &NewestImageUrl::errorOccurred, this,
          &ImageSelectPage::handleNewestImageUrlError);
  hasError = false;
}

void ImageSelectPage::initializePage() {
  if (!gondar::isChromeover()) {
    // for beerover, we'll have to check what the latest release is
    newestImageUrl->fetch();
  }
}

bool ImageSelectPage::validatePage() {
  // if there is an error, we need to allow the user to proceed to the error
  // screen
  if (hasError) {
    return true;
  }
  // currently this is only a concern in the chromeover case, but we would
  // be equally worried were this true in either case
  if (!bitnessButtons.checkedButton()) {
    return false;
  }
  if (!gondar::isChromeover()) {
    // in the beerover case, we need to have retrieved the latest image url
    return newestImageUrl->isReady();
  }
  return true;
}

int ImageSelectPage::nextId() const {
  return GondarWizard::Page_usbInsert;
}

void ImageSelectPage::addImage(GondarImage image) {
  // do not list deployable images
  if (image.isDeployable()) {
    return;
  }
  DownloadButton* newButton = new DownloadButton(image.url);
  newButton->setText(image.getCompositeName());
  bitnessButtons.addButton(newButton);
  layout.addWidget(newButton);
  if (image.is32Bit()) {
    layout.addWidget(&thirtyTwoDetails);
  }
}

void ImageSelectPage::addImages(QList<GondarImage> images) {
  for (const auto& curImage : images) {
    // FIXME: 32-bit images should be last.  For the time being, this must be
    // handled on the Gondar side.
    if (!curImage.is32Bit()) {
      addImage(curImage);
    }
  }
  for (const auto& curImage : images) {
    if (curImage.is32Bit()) {
      addImage(curImage);
    }
  }
}

// this is what is used later in the wizard to find what url should be used
QUrl ImageSelectPage::getUrl() {
  QAbstractButton* selected = bitnessButtons.checkedButton();
  if (gondar::isChromeover()) {
    // for chromeover, use the download button's url attribute
    DownloadButton* selected_download_button =
        dynamic_cast<DownloadButton*>(selected);
    return selected_download_button->getUrl();
  } else {
    // for beerover, we had to wait on a url lookup and we consult newestImage
    if (selected == &thirtyTwo) {
      return newestImageUrl->get32Url();
    } else if (selected == &sixtyFour) {
      return newestImageUrl->get64Url();
    } else {
      // TODO: decide what this behavior should be
      return newestImageUrl->get64Url();
    }
  }
}

void ImageSelectPage::handleNewestImageUrlError() {
  hasError = true;
  wizard()->postError("An error has occurred fetching the latest image");
}
