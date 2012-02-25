#ifndef REMOTESTREAM_H
#define REMOTESTREAM_H

// remotestream.h
// 2/15/2012

#include "inputstream.h"
#include "module/qtext/stoppable.h"
#include <QObject>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QRunnable>

class RemoteStream : public QObject, public InputStream,
                     public QRunnable, public Stoppable
{
  Q_OBJECT
  typedef RemoteStream Self;
  typedef QObject Base;

  QNetworkAccessManager *nam_;
  qint64 size_;
  QNetworkRequest request_;

public:
  explicit RemoteStream(QObject *parent = 0)
    : Base(parent), size_(0)
  { nam_ = new QNetworkAccessManager(this); }

signals:
  void finished();
  void readyRead();
  void progress(qint64 bytesReceived, qint64 bytesTotal);
  void error(QString message);

public:
  QNetworkAccessManager *networkAccessManager() const { return nam_; }
  const QNetworkRequest &request() const { return request_; }
  QNetworkRequest &request() { return request_; }
  QUrl url() const { return request_.url(); }
  virtual qint64 size() const { return size_; } ///< \override

  virtual QString contentType() const { return QString(); }

public slots:
  virtual void run() = 0; ///< \override as slot
  virtual void stop() = 0; ///< \override as slot

  void setRequest(const QNetworkRequest &req) { request_ = req; }
  void setUrl(const QUrl &url) { request_.setUrl(url); }
  void setSize(const qint64 &size) { size_ = size; }
};

#endif // IREMOTESTREAM_H
