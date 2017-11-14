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

#ifndef GONDARWIZARD_H
#define GONDARWIZARD_H

#include <memory>
#include <vector>

#include <QShortcut>
#include <QString>
#include <QWizard>

#include "device_picker.h"
#include "download_progress_page.h"
#include "image_select_page.h"
#include "usb_insert_page.h"
#include "write_operation_page.h"

class QCheckBox;
class QGroupBox;
class QLabel;
class QRadioButton;

class GondarSite;

class GondarWizard : public QWizard {
  Q_OBJECT

 public:
  GondarWizard(std::unique_ptr<gondar::DevicePicker> picker_in = nullptr,
               QWidget* parent = 0);

  ~GondarWizard();
  void init();

  int nextId() const override;
  void postError(const QString& error);
  qint64 getRunTime();
  void setMakeAnotherLayout();
  // There's an elaborate state-sharing solution via the 'field' mechanism
  // supported by QWizard.  I found the logic for that to be easy for sharing
  // some data types and convoluted for others.  In this case, a later page
  // makes a decision based on a radio button seleciton in an earlier page,
  // so putting the shared state in the wizard seems more straightforward
  ImageSelectPage imageSelectPage;
  DownloadProgressPage downloadProgressPage;
  UsbInsertPage usbInsertPage;
  WriteOperationPage writeOperationPage;

  const std::vector<GondarSite>& sites() const;
  void setSites(const std::vector<GondarSite>& sites);
  bool isFormatOnly() const { return formatOnly; }
  void setFormatOnly(bool newValue) { formatOnly = newValue; }

  // this enum determines page order
  enum {
    Page_adminCheck,
    Page_chromeoverLogin,
    Page_siteSelect,
    Page_imageSelect,
    Page_usbInsert,
    Page_deviceSelect,
    Page_downloadProgress,
    Page_writeOperation,
    Page_error
  };

 private slots:
  void handleCustomButton(int buttonIndex);

 private:
  class Private;
  std::unique_ptr<Private> p_;

  void catchError(const QString& error);
  // Set the button layout appropriate for most pages; no 'make another usb'
  // button.
  // The default layout includes a back button, which is now omitted.
  void setNormalLayout();

  QShortcut about_shortcut_;
  bool formatOnly;
};

#endif /* GONDARWIZARD */
