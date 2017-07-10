
#ifndef GONDARWIZARD_H
#define GONDARWIZARD_H

#include <QProgressBar>
#include <QRadioButton>
#include <QString>
#include <QWizard>
#include <QtWidgets>

#include "admin_check_page.h"
#include "deviceguy.h"
#include "diskwritethread.h"
#include "downloader.h"
#include "image_select_page.h"
#include "unzipthread.h"
#include "wizard_page.h"

class QCheckBox;
class QGroupBox;
class QLabel;
class QRadioButton;

class GondarButton : public QRadioButton {
  Q_OBJECT

 public:
  GondarButton(const QString& text,
               unsigned int device_num,
               QWidget* parent = 0);
  unsigned int index = 0;
};

class DownloadProgressPage : public gondar::WizardPage {
  Q_OBJECT

 public:
  DownloadProgressPage(QWidget* parent = 0);
  bool isComplete() const override;
  const QString& getImageFileName();

 protected:
  void initializePage() override;
  void notifyUnzip();

 public slots:
  void markComplete();
  void downloadProgress(qint64 sofar, qint64 total);
  void onDownloadStarted();
  void onUnzipFinished();

 private:
  bool range_set;
  DownloadManager manager;
  QProgressBar progress;
  bool download_finished;
  QVBoxLayout layout;
  UnzipThread* unzipThread;
};

class UsbInsertPage : public gondar::WizardPage {
  Q_OBJECT

 public:
  UsbInsertPage(QWidget* parent = 0);

 protected:
  void initializePage() override;
  bool isComplete() const override;

 private:
  void showDriveList();
  QLabel label;
  QTimer* tim;
  QVBoxLayout layout;

 public slots:
  void getDriveList();
 signals:
  void driveListRequested();
};

class DeviceSelectPage : public gondar::WizardPage {
  Q_OBJECT

 public:
  DeviceSelectPage(QWidget* parent = 0);
  int nextId() const override;


 protected:
  void initializePage() override;
  bool validatePage() override;

 private:
  QLabel drivesLabel;
  QGroupBox* drivesBox;
  QButtonGroup* radioGroup;
  QVBoxLayout* layout;
};

class WriteOperationPage : public gondar::WizardPage {
  Q_OBJECT

 public:
  WriteOperationPage(QWidget* parent = 0);

 protected:
  void initializePage() override;
  bool isComplete() const override;
  bool validatePage() override;
  void showProgress();
  int nextId() const override;
  void setVisible(bool visible) override;
 public slots:
  void onDoneWriting();

 private:
  void writeToDrive();
  QVBoxLayout layout;
  QProgressBar progress;
  bool writeFinished;
  DiskWriteThread* diskWriteThread;
  QString image_path;
};

class ErrorPage : public gondar::WizardPage {
  Q_OBJECT

 public:
  ErrorPage(QWidget* parent = 0);
  void setErrorString(QString errorString);
  bool errorEmpty() const;
 protected:
  void initializePage() override;
  int nextId() const override;
  void setVisible(bool visible) override;
 private:
  QVBoxLayout layout;
  QString errorString;
  QLabel label;
};

class GondarWizard : public QWizard {
  Q_OBJECT

 public:
  GondarWizard(QWidget* parent = 0);

  void goToErrorPage(QString errorStringIn);
  int nextId() const override;
  void postError(const QString& error);
  void catchError(const QString& error);
  // There's an elaborate state-sharing solution via the 'field' mechanism
  // supported by QWizard.  I found the logic for that to be easy for sharing
  // some data types and convoluted for others.  In this case, a later page
  // makes a decision based on a radio button seleciton in an earlier page,
  // so putting the shared state in the wizard seems more straightforward
  AdminCheckPage adminCheckPage;
  ErrorPage errorPage;
  ImageSelectPage imageSelectPage;
  DownloadProgressPage downloadProgressPage;
  UsbInsertPage usbInsertPage;
  DeviceSelectPage deviceSelectPage;
  WriteOperationPage writeOperationPage;

  // this enum determines page order
  enum {
    Page_adminCheck,
    Page_imageSelect,
    Page_usbInsert,
    Page_deviceSelect,
    Page_downloadProgress,
    Page_writeOperation,
    Page_error
  };
 private slots:
  void handleMakeAnother();
};

#endif /* GONDARWIZARD */
