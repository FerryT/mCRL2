// Author(s): Ferry Timmers
// Copyright: see the accompanying file COPYING or copy at
// https://github.com/mCRL2org/mCRL2/blob/master/COPYING
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef DETACHEDLOG_H
#define DETACHEDLOG_H

#include <QObject>
#include <QFileSystemWatcher>

/// Logging class for detached processes
/// Opens temporary files for std output and error, which can be written to,
/// which causes signals with the updated text
class DetachedLog : public QObject
{
    Q_OBJECT

  class Channel
  {
  	public:
  		QString filename;
  		qint64 offset;
  		QFileSystemWatcher watcher;

  		Channel(const QString &pattern);
  		~Channel();
  };

  public:
    DetachedLog();
    virtual ~DetachedLog();

    QString outputFilename() const { return m_output.filename; };
    QString errorFilename() const { return m_error.filename; };
    void reset() { m_output.offset = m_error.offset = 0; }

  signals:
    void readyOutputLog(const QByteArray &text);
    void readyErrorLog(const QByteArray &text);

  private:
    Channel m_output;
    Channel m_error;

    static QByteArray receiveLog(Channel &channel);

  private slots:
    void onOutputFileChanged(const QString &path);
    void onErrorFileChanged(const QString &path);
};

#endif // DETACHEDLOG_H
