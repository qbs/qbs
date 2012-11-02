/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "persistence.h"

#include "fileinfo.h"
#include <logging/logger.h>
#include <tools/error.h>

#include <QDir>
#include <QScopedPointer>

namespace qbs {

static const char QBS_PERSISTENCE_MAGIC[] = "QBSPERSISTENCE0_0_1__19";

PersistentPool::PersistentPool()
{
}

PersistentPool::~PersistentPool()
{
    closeStream();
}

bool PersistentPool::load(const QString &filePath)
{
    QScopedPointer<QFile> file(new QFile(filePath));
    if (!file->open(QFile::ReadOnly)) {
        qbsDebug() << "Could not open build graph file for reading: " << file->errorString();
        return false;
    }

    m_stream.setDevice(file.data());
    QByteArray magic;
    m_stream >> magic;
    if (magic != QBS_PERSISTENCE_MAGIC) {
        file->close();
        file->remove();
        m_stream.setDevice(0);
        qbsInfo() << "Cannot use stored build graph: Incompatible file format.";
        return false;
    }

    m_stream >> m_headData.projectConfig;
    file.take();
    m_loadedRaw.clear();
    m_loaded.clear();
    m_storageIndices.clear();
    m_stringStorage.clear();
    m_inverseStringStorage.clear();
    return true;
}

bool PersistentPool::setupWriteStream(const QString &filePath)
{
    QString dirPath = FileInfo::path(filePath);
    if (!FileInfo::exists(dirPath))
        if (!QDir().mkpath(dirPath))
            return false;

    if (QFile::exists(filePath) && !QFile::remove(filePath))
        return false;
    Q_ASSERT(!QFile::exists(filePath));
    QScopedPointer<QFile> file(new QFile(filePath));
    if (!file->open(QFile::WriteOnly))
        return false;

    m_stream.setDevice(file.take());
    m_stream << QByteArray(QBS_PERSISTENCE_MAGIC) << m_headData.projectConfig;
    m_lastStoredObjectId = 0;
    m_lastStoredStringId = 0;
    return true;
}

void PersistentPool::closeStream()
{
    delete m_stream.device();
    m_stream.setDevice(0);
}

void PersistentPool::store(const PersistentObject *object)
{
    if (!object) {
        m_stream << -1;
        return;
    }
    PersistentObjectId id = m_storageIndices.value(object, -1);
    if (id < 0) {
        id = m_lastStoredObjectId++;
        m_storageIndices.insert(object, id);
        m_stream << id;
        object->store(*this);
    } else {
        m_stream << id;
    }
}

void PersistentPool::clear()
{
    m_loaded.clear();
    m_storageIndices.clear();
    m_stringStorage.clear();
    m_inverseStringStorage.clear();
}

QDataStream &PersistentPool::stream()
{
    return m_stream;
}

void PersistentPool::storeString(const QString &t)
{
    int id = m_inverseStringStorage.value(t, -1);
    if (id < 0) {
        id = m_lastStoredStringId++;
        m_inverseStringStorage.insert(t, id);
        m_stream << id << t;
    } else {
        m_stream << id;
    }
}

QString PersistentPool::loadString(int id)
{
    if (id < 0)
        throw Error("loadString with negative id called");

    if (id >= m_stringStorage.count()) {
        QString s;
        m_stream >> s;
        m_stringStorage.resize(id + 1);
        m_stringStorage[id] = s;
        return s;
    }

    return m_stringStorage.at(id);
}

QString PersistentPool::idLoadString()
{
    int id;
    m_stream >> id;
    return loadString(id);
}

void PersistentPool::storeStringSet(const QSet<QString> &t)
{
    m_stream << t.count();
    foreach (const QString &s, t)
        storeString(s);
}

QSet<QString> PersistentPool::idLoadStringSet()
{
    int i;
    m_stream >> i;
    QSet<QString> result;
    for (; --i >= 0;)
        result += idLoadString();
    return result;
}

void PersistentPool::storeStringList(const QStringList &t)
{
    m_stream << t.count();
    foreach (const QString &s, t)
        storeString(s);
}

QStringList PersistentPool::idLoadStringList()
{
    int i;
    m_stream >> i;
    QStringList result;
    for (; --i >= 0;)
        result += idLoadString();
    return result;
}

} // namespace qbs
