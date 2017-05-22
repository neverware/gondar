
#ifndef GONDARWIZARD_H
#define GONDARWIZARD_H

#include <QWizard>
#include <QRadioButton>

#include <QString>

#include "downloader.h"

extern "C" {
    #include "deviceguy.h"
}

class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QRadioButton;

class GondarButton : public QRadioButton {
    Q_OBJECT

public:
    GondarButton(const QString & text, 
                 unsigned int device_num,
                 QWidget *parent = 0);
    unsigned int index = 0;
};

class GondarWizard : public QWizard
{
    Q_OBJECT

public:
    GondarWizard(QWidget *parent = 0);

};

class AdminCheckPage : public QWizardPage
{
    Q_OBJECT

public:
    AdminCheckPage(QWidget *parent = 0);

protected:
    void initializePage() override;
    bool isComplete() const;

public slots:
    void getIsAdmin();
    void showIsAdmin();
    void showIsNotAdmin();
signals:
    void isAdminRequested();
    void isAdminReady();
    void isNotAdminReady();
private:
    QLabel *label;
    QTimer *tim;
    bool is_admin;
};

class IntroPage : public QWizardPage
{
    Q_OBJECT

public:
    IntroPage(QWidget *parent = 0);

protected:
    void initializePage() override;
    bool isComplete() const;

private:
    QLabel *label;
    QTimer *tim;

public slots:
    void getDriveList();
    void showDriveList();
signals:
    void driveListRequested();
    void driveListReady();
};

class ConclusionPage : public QWizardPage
{
    Q_OBJECT

public:
    ConclusionPage(QWidget *parent = 0);

protected:
    void initializePage() override;
    bool validatePage() override;

private:
    QLabel *label;
    //DownloadManager manager;
    QLabel *drivesLabel;
    QLineEdit *urlLineEdit;
    QGroupBox *drivesBox;
    QButtonGroup * radioGroup;
};

class KewlPage: public QWizardPage {
    Q_OBJECT

public:
    KewlPage(QWidget *parent = 0);

protected:
    void initializePage() override;
    bool validatePage() override;

public slots:
    void writeToDrive();
signals:
    void WriteDriveRequested();
};

#endif /* GONDARWIZARD */
