#ifndef ADMIN_CHECK_PAGE_H
#define ADMIN_CHECK_PAGE_H

#include <QLabel>
#include <QVBoxLayout>
#include <QWizardPage>

class AdminCheckPage : public QWizardPage {
  Q_OBJECT

 public:
  AdminCheckPage(QWidget* parent = 0);

 protected:
  void initializePage() override;
  bool isComplete() const override;
  void showIsAdmin();
  void showIsNotAdmin();

 private:
  QLabel label;
  bool is_admin;
  QVBoxLayout layout;
};

#endif