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

#include <QLabel>
#include <QRadioButton>

#include "gondarsite.h"
#include "gondarwizard.h"
#include "log.h"
#include "metric.h"

class SiteButton : public QRadioButton {
 public:
  explicit SiteButton(const GondarSite& site) : site_(site) {
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

// TODO(ken): background: i hate OO
// given this, does it make more sense for this to be outside the class
// and take the page and siteButtons as args so it's more clear what
// instance members it depends on?
void SiteSelectPage::updateSitesForPage() {
  // for page 1, 0
  // for page 2, 5
  int min = (page - 1) * 5;
  // for page 1, 5
  // for page 2, 10
  int max = page * 5;
  int itr = 0;
  // assumes sitesButtons has already been populated
  for (const auto &button : sitesButtons.buttons()) {
    if (itr >= min && itr < max) {
      // ideally we would not instantiate the sitebuttons.
      // we will need a separate vector of sitebuttons
      // and then we will add/remove them from sitesButtons list?
      // or we just toggle them invisible?  that seems simpler
      button->setVisible(true);
    } else {
      button->setVisible(false);
    }
    itr++;
  }
  pageGroup.setTitle(QString("Page %1 / %2").arg(page).arg(getTotalPages()));
}

int SiteSelectPage::getTotalPages() {
  int total_sites = wizard()->sites().size();
  // something like that, not sure what cleanest heuristic is
  int total_pages = (total_sites + 4) / 5;
  return total_pages;
}

void SiteSelectPage::initializePage() {
  const auto& sitesList = wizard()->sites();
  for (const auto& site : sitesList) {
    auto* curButton = new SiteButton(site);
    sitesButtons.addButton(curButton);
    layout.addWidget(curButton);
  }
  page = 1;
  bool lots_of_sites = false;
  if (wizard()->sites().size() > 5) {
    lots_of_sites = true;
  }
  pageGroup.setVisible(lots_of_sites);
  prevPageButton.setText("Previous");
  nextPageButton.setText("Next");
  layout.addWidget(&pageGroup);
  pageNavLayout.addWidget(&prevPageButton);
  pageNavLayout.addWidget(&nextPageButton);
  //layout.addLayout(&pageNavLayout);
  pageGroup.setLayout(&pageNavLayout);
  connect(&nextPageButton, &QPushButton::clicked, this,
          &SiteSelectPage::handleNextPage);
  connect(&prevPageButton, &QPushButton::clicked, this,
          &SiteSelectPage::handlePrevPage);
  // limit visible sites to those on this page
  updateSitesForPage();
}

void SiteSelectPage::handleNextPage() {
  if (page < getTotalPages()) {
    page++;
    updateSitesForPage();
  }
}

void SiteSelectPage::handlePrevPage() {
  if (page > 1) {
    page--;
    updateSitesForPage();
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
    // metrics may now include site id
    gondar::SetSiteId(site.getSiteId());
    QList<GondarImage> imageList = site.getImages();
    wizard()->imageSelectPage.addImages(imageList);
    return true;
  }
}
