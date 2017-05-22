
#include <QtWidgets>

#include "gondarwizard.h"
#include "downloader.h"

extern "C" {
  #include "gondar.h"
  #include "deviceguy.h"
}

//FIXME(kendall): move this into the wizard and send signals to individual
// pages
DeviceGuyList * drivelist = NULL;
DeviceGuy * selected_drive = NULL;

bool writeFinished = false;

GondarButton::GondarButton(const QString & text,
                           unsigned int device_num,
                           QWidget *parent)
                           : QRadioButton(text, parent) {
    index = device_num;
    
}
GondarWizard::GondarWizard(QWidget *parent)
    : QWizard(parent)
{
    addPage(new AdminCheckPage);
    addPage(new IntroPage);
    addPage(new ConclusionPage);
    addPage(new KewlPage);
    setWindowTitle(tr("Cloudready USB Creation Utility"));
}

AdminCheckPage::AdminCheckPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Insert USB Drive"));
    setPixmap(QWizard::WatermarkPixmap, QPixmap(":/images/frogmariachis.png"));

    label = new QLabel("Please wait...");
    is_admin = false; // assume false until we discover otherwise.
                      // this holds the user at this screen

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(label);
    setLayout(layout);

    // the next button is grayed out if user does not have appropriate rights
    QObject::connect(this, SIGNAL(isAdminRequested()),
                     this, SLOT(getIsAdmin()));
    QObject::connect(this, SIGNAL(isAdminReady()),
                     this, SLOT(showIsAdmin()));
    QObject::connect(this, SIGNAL(isNotAdminReady()),
                     this, SLOT(showIsNotAdmin()));
}

void AdminCheckPage::initializePage() {
    tim = new QTimer(this);
    connect(tim, SIGNAL(timeout()), SLOT(getIsAdmin()));
    emit isAdminRequested();
}

bool AdminCheckPage::isComplete() const {
    return is_admin;
}

void AdminCheckPage::getIsAdmin() {
    qDebug() << "kendall: getIsAdmin fires";
    is_admin = IsCurrentProcessElevated();
    if (!is_admin) {
        emit isNotAdminReady(); 
        tim->stop();
    } else {
        tim->stop();
        emit isAdminReady();
    }
}

void AdminCheckPage::showIsAdmin() {
    label->setText("User has admin rights.");
    emit completeChanged();
}

void AdminCheckPage::showIsNotAdmin() {
    label->setText("User does not have admin rights.");
}

IntroPage::IntroPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Insert USB Drive"));
    setPixmap(QWizard::WatermarkPixmap, QPixmap(":/images/frogmariachis.png"));

    label = new QLabel("Please insert the destination USB drive to create a "
                        "USB Cloudready(tm) bootable USB drive.");;
    label->setWordWrap(true);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(label);
    setLayout(layout);

    // the next button should be grayed out until the user inserts a USB
    QObject::connect(this, SIGNAL(driveListRequested()),
                     this, SLOT(getDriveList()));
    QObject::connect(this, SIGNAL(driveListReady()),
                     this, SLOT(showDriveList()));
}

void IntroPage::initializePage() {
    tim = new QTimer(this);
    //tim->setSingleShot(true);
    connect(tim, SIGNAL(timeout()), SLOT(getDriveList()));
    // send a signal to check for drives
    emit driveListRequested();
}

bool IntroPage::isComplete() const {
    // this should return false unless we have a non-empty result from
    // GetDevices()
    if (drivelist == NULL) {
        return false;
    }
    else {
        return true;
    }
}

void IntroPage::getDriveList() {
    qDebug() << "kendall: getDriveList fires";
    drivelist = GetDeviceList();
    qDebug() << "kendall: we finish getting the drivelist";
    if (DeviceGuyList_length(drivelist) == 0) {
        DeviceGuyList_free(drivelist); 
        drivelist = NULL;
        tim->start(1000);
    } else {
        tim->stop();
        emit driveListReady();
    }
}

void IntroPage::showDriveList() {
    qDebug() << "kendall: in showDriveList";
    emit completeChanged();
}

ConclusionPage::ConclusionPage(QWidget *parent)
    : QWizardPage(parent)
{
    // this page should just say 'hi how are you' while it stealthily loads
    // the usb device list.  or it could ask you to insert your device
    setTitle(tr("Select Drive"));
    setPixmap(QWizard::WatermarkPixmap, QPixmap(":/images/frogmariachis.png"));

}

void ConclusionPage::initializePage()
{
    label = new QLabel("Target image url:");
    urlLineEdit = new QLineEdit;
    registerField("imageurl", urlLineEdit);
    drivesLabel = new QLabel("Select Drive:");
    if (drivelist == NULL) {
        qDebug() << "kendall: drivelist was null?!";
        return;
    }
    DeviceGuy * itr = drivelist->head;
    // Line up widgets horizontally
    // use QVBoxLayout for vertically, H for horizontal
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(label);
    layout->addWidget(urlLineEdit);
    layout->addWidget(drivesLabel);

    radioGroup = new QButtonGroup();
    // i could extend the button object to also have a secret index
    // then i could look up index later easily
    while (itr != NULL) {
        GondarButton * cur_radio = new GondarButton(itr->name,
                                                    itr->device_num,
                                                    this);
        radioGroup->addButton(cur_radio);
        layout->addWidget(cur_radio);
        itr = itr->next;
    }
    setLayout(layout);
}

bool ConclusionPage::validatePage() {
    //TODO(kendall): check for NULL on bad cast
    GondarButton * selected = dynamic_cast<GondarButton *>(radioGroup->checkedButton());
    if (selected == NULL) {
        return false;
    } else {
        //FIXME(kendall): what we actually want here afterall is a DeviceGuy.
        unsigned int selected_index = selected->index;
        selected_drive = DeviceGuyList_getByIndex(drivelist, selected_index);
        return true;
    }
}

KewlPage::KewlPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Writing to disk will like, totally wipe your drive, dude."));
    QObject::connect(this, SIGNAL(WriteDriveRequested()),
                     this, SLOT(WriteToDrive()));
}

void KewlPage::initializePage()
{
    qDebug() << "drive name: " << selected_drive->name << ", index: " << selected_drive->device_num;
}

bool KewlPage::validatePage() {
    qDebug() << "in validatePage";
    if (selected_drive == NULL) {
        qDebug() << "ERROR: no drive selected";
    } else {
        emit WriteDriveRequested();
    }
    return writeFinished;
}

void KewlPage::writeToDrive() {
    static bool isWriting = false;
    qDebug() << "received request to write";
    if (!isWriting) {
        qDebug() << "actually starting install!";
        isWriting = true;
        char image_path[] = "c:\\cloudready.bin";
        Install(selected_drive, image_path);
        qDebug() << "install call returned";
        writeFinished = true;
    }
}
