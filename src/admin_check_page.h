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

#ifndef ADMIN_CHECK_PAGE_H
#define ADMIN_CHECK_PAGE_H

#include <QLabel>
#include <QVBoxLayout>

#include "wizard_page.h"

class AdminCheckPage : public gondar::WizardPage {
  Q_OBJECT

 public:
  AdminCheckPage(QWidget* parent = 0);
  int nextId() const override;
  void handleFormatOnly();

 protected:
  void initializePage() override;
  bool isComplete() const override;
  void showIsAdmin();
  void showIsNotAdmin();

 private:
  QLabel label;
  // offer the users a way to format their disk
  QLabel warpTunnel;
  bool is_admin;
  QVBoxLayout layout;
};

#endif
