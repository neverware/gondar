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

#include "wizard_page.h"

#include "gondarwizard.h"

namespace gondar {

WizardPage::WizardPage(QWidget* parent) : QWizardPage(parent) {}

GondarWizard* WizardPage::wizard() const {
  QWizard* base_wiz = QWizardPage::wizard();
  Q_ASSERT(base_wiz);
  GondarWizard* gee_wiz = dynamic_cast<GondarWizard*>(base_wiz);
  Q_ASSERT(gee_wiz);
  return gee_wiz;
}
}  // namespace gondar
