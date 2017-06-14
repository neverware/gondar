
#ifndef UNZIP_THREAD_H
#define UNZIP_THREAD_H

#include <QThread>
#include <QUrl>

class UnzipThread : public QThread {
    Q_OBJECT
  public:
    UnzipThread(const QUrl * url_in, QObject *parent = 0);
    ~UnzipThread();

  protected:
    void run() override;
    QUrl url;
};

#endif /* UNZIP_THREAD_H */