
#include <QtWidgets>

#include "gondarwizard.h"
#include "downloader.h"

extern "C" {
  #include "gondar.h"
  #include "deviceguy.h"
  #include "neverware_unzipper.h"
}

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
    addPage(new ImageSelectPage);
    addPage(new DownloadProgressPage);
    addPage(new UsbInsertPage);
    addPage(new DeviceSelectPage);
    addPage(new KewlPage);
    setWindowTitle(tr("Cloudready USB Creation Utility"));
}

AdminCheckPage::AdminCheckPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Insert USB Drive"));

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

ImageSelectPage::ImageSelectPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Select Remote Image"));
}

void ImageSelectPage::initializePage() {
    label = new QLabel("Target image url:");
    urlLineEdit = new QLineEdit;
    //TODO(kendall): make required
    registerField("imageurl", urlLineEdit);
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(label);
    layout->addWidget(urlLineEdit);
    setLayout(layout);
}

DownloadProgressPage::DownloadProgressPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Select Remote Image"));
    download_finished = false;
}

void DownloadProgressPage::initializePage() {
    label = new QLabel("Downloading...");
    qDebug() << "url = " << field("imageurl");
    QObject::connect(&manager, SIGNAL(finished()), this, SLOT(markComplete()));
    manager.append(field("imageurl").toString());
}

void DownloadProgressPage::markComplete() {
    download_finished = true;
    label->setText("Download is complete.");
    // now that the download is finished, let's unzip the build.
    const char * url = field("imageurl").toString().toStdString().c_str();
    neverware_unzip(url);
    emit completeChanged();
}

bool DownloadProgressPage::isComplete() const {
    return download_finished;
}

UsbInsertPage::UsbInsertPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Insert USB Drive"));

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

void UsbInsertPage::initializePage() {
    tim = new QTimer(this);
    //tim->setSingleShot(true);
    connect(tim, SIGNAL(timeout()), SLOT(getDriveList()));
    // send a signal to check for drives
    emit driveListRequested();
}

bool UsbInsertPage::isComplete() const {
    // this should return false unless we have a non-empty result from
    // GetDevices()
    if (drivelist == NULL) {
        return false;
    }
    else {
        return true;
    }
}

void UsbInsertPage::getDriveList() {
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

void UsbInsertPage::showDriveList() {
    qDebug() << "kendall: in showDriveList";
    emit completeChanged();
}

DeviceSelectPage::DeviceSelectPage(QWidget *parent)
    : QWizardPage(parent)
{
    // this page should just say 'hi how are you' while it stealthily loads
    // the usb device list.  or it could ask you to insert your device
    setTitle(tr("Select Drive"));

}

void DeviceSelectPage::initializePage()
{
    drivesLabel = new QLabel("Select Drive:");
    if (drivelist == NULL) {
        qDebug() << "kendall: drivelist was null?!";
        return;
    }
    DeviceGuy * itr = drivelist->head;
    // Line up widgets horizontally
    // use QVBoxLayout for vertically, H for horizontal
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(drivesLabel);

    radioGroup = new QButtonGroup();
    // i could extend the button object to also have a secret index
    // then i could look up index later easily
    while (itr != NULL) {
        GondarButton * cur_radio = new GondarButton(itr->name,
                                                    itr->device_num,
                                                    this);
        radioGroup->addButton(cur_radio);
        //FIXME(kendall): occassionally get a warning about null pointer
        layout->addWidget(cur_radio);
        itr = itr->next;
    }
    setLayout(layout);
}

bool DeviceSelectPage::validatePage() {
    //TODO(kendall): check for NULL on bad cast
    GondarButton * selected = dynamic_cast<GondarButton *>(radioGroup->checkedButton());
    if (selected == NULL) {
        return false;
    } else {
        unsigned int selected_index = selected->index;
        selected_drive = DeviceGuyList_getByIndex(drivelist, selected_index);
        return true;
    }
}

KewlPage::KewlPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Writing to disk will like, totally wipe your drive, dude."));
    QObject::connect(this, SIGNAL(writeDriveRequested()),
                     this, SLOT(writeToDrive()));
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
        qDebug() << "emitting WriteDriveRequested()";
        emit writeDriveRequested();
    }
    return writeFinished;
}

void KewlPage::writeToDrive() {
    static bool isWriting = false;
    qDebug() << "received request to write";
    if (!isWriting) {
        qDebug() << "actually starting install!";
        isWriting = true;
        char image_path[] = "chromiumos_image.bin";
        Install(selected_drive, image_path);
        qDebug() << "install call returned";
        writeFinished = true;
    }
}
