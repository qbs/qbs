/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include "persistence.h"
#include "fileinfo.h"
#include <QtCore/QDir>
#include <stdio.h>

namespace qbs {

static const char QBS_PERSISTENCE_MAGIC[] = "QBSPERSISTENCE0_0_1__6";

PersistentPool::PersistentPool()
{
}

PersistentPool::~PersistentPool()
{
}

bool PersistentPool::load(const QString &filePath, const LoadMode loadMode)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
        return false;
    QDataStream s(&file);

    QByteArray magic;
    s >> magic;
    if (magic != QBS_PERSISTENCE_MAGIC) {
        file.close();
        file.remove();
        return false;
    }

    s >> m_headData.projectConfig;
    if (loadMode == LoadHeadData)
        return true;

    s >> m_stringStorage;
    m_inverseStringStorage.reserve(m_stringStorage.count());
    for (int i = m_stringStorage.count(); --i >= 0;)
        m_inverseStringStorage.insert(m_stringStorage.at(i), i);

    s >> m_storage;
    m_loadedRaw.reserve(m_storage.count());
    m_loaded.reserve(m_storage.count());
    return true;
}

bool PersistentPool::store(const QString &filePath)
{
    QString dirPath = FileInfo::path(filePath);
    if (!FileInfo::exists(dirPath))
        if (!QDir().mkpath(dirPath))
            return false;

    if (QFile::exists(filePath) && !QFile::remove(filePath))
        return false;
    Q_ASSERT(!QFile::exists(filePath));
    QFile file(filePath);
    if (!file.open(QFile::WriteOnly))
        return false;

    QDataStream s(&file);
    s << QByteArray(QBS_PERSISTENCE_MAGIC);
    s << m_headData.projectConfig;
    s << m_stringStorage;
    s << m_storage;
    return true;
}

PersistentObjectId PersistentPool::store(PersistentObject *object)
{
    if (!object)
        return 0;
    PersistentObjectId id = m_storageIndices.value(object, -1);
    if (id < 0) {
        PersistentObjectData data;
        id = qMax(m_storage.count(), m_maxReservedId + 1);
        m_storageIndices.insert(object, id);
        m_storage.resize(id + 1);
        m_storage[id] = data;
        object->store(*this, data);
        m_storage[id] = data;
    }
    return id;
}

void PersistentPool::clear()
{
    m_loaded.clear();
    m_storage.clear();
    m_storageIndices.clear();
    m_stringStorage.clear();
    m_inverseStringStorage.clear();
}

PersistentObjectData PersistentPool::getData(PersistentObjectId id) const
{
    return m_storage.at(id);
}

void PersistentPool::setData(PersistentObjectId id, PersistentObjectData data)
{
    Q_ASSERT(id <= m_maxReservedId);
    if (id >= m_storage.count())
        m_storage.resize(id + 1);
    m_storage[id] = data;
}

int PersistentPool::storeString(const QString &t)
{
    int id = m_inverseStringStorage.value(t, -1);
    if (id < 0) {
        id = m_stringStorage.count();
        m_inverseStringStorage.insert(t, id);
        m_stringStorage.append(t);
    }
    return id;
}

QString PersistentPool::loadString(int id)
{
    if (id < 0 || id >= m_stringStorage.count())
        throw Error(QString("storage error: no string id %1 stored.").arg(id));

    return m_stringStorage.at(id);
}

QString PersistentPool::idLoadString(QDataStream &s)
{
    int id;
    s >> id;
    return loadString(id);
}

QList<int> PersistentPool::storeStringSet(const QSet<QString> &t)
{
    QList<int> r;
    r.reserve(t.count());
    foreach (const QString &s, t)
        r += storeString(s);
    return r;
}

QSet<QString> PersistentPool::loadStringSet(const QList<int> &ids)
{
    QSet<QString> r;
    r.reserve(ids.count());
    foreach (int id, ids)
        r.insert(loadString(id));
    return r;
}

QSet<QString> PersistentPool::idLoadStringSet(QDataStream &s)
{
    QList<int> list;
    s >> list;
    return loadStringSet(list);
}

QList<int> PersistentPool::storeStringList(const QStringList &t)
{
    QList<int> r;
    r.reserve(t.count());
    foreach (const QString &s, t)
        r += storeString(s);
    return r;
}

QStringList PersistentPool::loadStringList(const QList<int> &ids)
{
    QStringList result;
    result.reserve(ids.count());
    foreach (const int &id, ids)
        result.append(loadString(id));
    return result;
}

QStringList PersistentPool::idLoadStringList(QDataStream &s)
{
    QList<int> list;
    s >> list;
    return loadStringList(list);
}

} // namespace qbs
