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

#include "site_select_page.h"

#include <QRadioButton>

#include "gondarsite.h"
#include "gondarwizard.h"

class SiteButton : public QRadioButton {
 public:
  SiteButton(const GondarSite& site) : site_(site) {
    setText(site.getSiteName());
  }

  const GondarSite& site() const { return site_; }

 private:
  const GondarSite site_;
};

SiteSelectPage::SiteSelectPage(QWidget* parent) : WizardPage(parent) {
  setTitle("Site Select");
  setSubTitle(
      "Your account is associated with more than one site. "
      "Select the site you'd like to use.");
  setLayout(&layout);
}

void SiteSelectPage::initializePage() {
  const auto& sitesList = wizard()->sites();
  for (const auto& site : sitesList) {
    auto* curButton = new SiteButton(site);
    sitesButtons.addButton(curButton);
    layout.addWidget(curButton);
  }
}

bool SiteSelectPage::validatePage() {
  // if we have a site selected, update our download links and continue
  // otherwise, return false
  auto* selected = dynamic_cast<SiteButton*>(sitesButtons.checkedButton());
  if (selected == NULL) {
    return false;
  } else {
    const auto site = selected->site();
    QList<GondarImage> imageList = site.getImages();
    wizard()->imageSelectPage->addImages(imageList);
    return true;
  }
}
