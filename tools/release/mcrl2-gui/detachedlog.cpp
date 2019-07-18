// Author(s): Ferry Timmers
// Copyright: see the accompanying file COPYING or copy at
// https://github.com/mCRL2org/mCRL2/blob/master/COPYING
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include <QDir>
#include <QProcess>
#include <QTemporaryFile>

#include "detachedlog.h"

DetachedLog::Channel::Channel(const QString &pattern)
  : filename(QProcess::nullDevice()), offset(0)
{
  QTemporaryFile temp(QDir(QDir::tempPath()).filePath(pattern));
  if (temp.open())
  {
    temp.setAutoRemove(false);
    temp.close();
    filename = temp.fileName();
    watcher.addPath(filename);
  }
}

DetachedLog::Channel::~Channel()
{
  if (filename != QProcess::nullDevice())
    QFile::remove(filename);
}

DetachedLog::DetachedLog()
  : m_output("mcrl2-gui-XXXXXX.out"), m_error("mcrl2-gui-XXXXXX.err")
{
  // Bug in QFileSystemWatcher (Windows):
  // it currently only triggers when a modified file is closed.
  // maybe use stat instead?
  connect(&m_output.watcher, SIGNAL(fileChanged(QString)), this,
    SLOT(onOutputFileChanged(QString)));
  connect(&m_error.watcher, SIGNAL(fileChanged(QString)), this,
    SLOT(onErrorFileChanged(QString)));
}

DetachedLog::~DetachedLog()
{
}

QByteArray DetachedLog::receiveLog(DetachedLog::Channel &channel)
{
  QByteArray text;
  QFile file(channel.filename);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    file.seek(channel.offset);
    text = file.readAll();
    channel.offset = file.pos();
    file.close();
  }
  return text;
}

void DetachedLog::onOutputFileChanged(const QString &path)
{
  QByteArray text = receiveLog(m_output);
  if (!text.isEmpty())
    emit(readyOutputLog(text));
}

void DetachedLog::onErrorFileChanged(const QString &path)
{
  QByteArray text = receiveLog(m_error);
  if (!text.isEmpty())
    emit(readyErrorLog(text));
}
