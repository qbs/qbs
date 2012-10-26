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

#ifndef QBS_PERSISTENCE
#define QBS_PERSISTENCE

#include "persistentobject.h"

#include <QDataStream>
#include <QSharedPointer>
#include <QString>
#include <QVariantMap>
#include <QVector>

namespace qbs {

typedef int PersistentObjectId;
class PersistentPool;

class PersistentPool
{
public:
    PersistentPool();
    ~PersistentPool();

    struct HeadData
    {
        QVariantMap projectConfig;
    };

    bool load(const QString &filePath);
    bool setupWriteStream(const QString &filePath);
    void closeStream();
    void clear();
    QDataStream &stream();

    template <typename T>
    inline T *idLoad()
    {
        PersistentObjectId id;
        stream() >> id;
        return loadRaw<T>(id);
    }

    template <class T>
    inline T *loadRaw(PersistentObjectId id)
    {
        if (id < 0)
            return 0;

        PersistentObject *obj = 0;
        if (id < m_loadedRaw.count()) {
            obj = m_loadedRaw.value(id);
        } else {
            int i = m_loadedRaw.count();
            m_loadedRaw.resize(id + 1);
            for (; i < m_loadedRaw.count(); ++i)
                m_loadedRaw[i] = 0;

            obj = new T;
            m_loadedRaw[id] = obj;
            obj->load(*this);
        }

        return static_cast<T*>(obj);
    }

    template <class T>
    inline typename T::Ptr idLoadS()
    {
        PersistentObjectId id;
        m_stream >> id;
        return load<T>(id);
    }

    template <class T>
    inline QSharedPointer<T> load(PersistentObjectId id)
    {
        if (id < 0)
            return QSharedPointer<T>();

        QSharedPointer<PersistentObject> obj;
        if (id < m_loaded.count()) {
            obj = m_loaded.value(id);
        } else {
            m_loaded.resize(id + 1);
            obj = T::create();
            m_loaded[id] = obj;
            obj->load(*this);
        }

        return obj.staticCast<T>();
    }

    template <class T>
    void store(QSharedPointer<T> ptr)
    {
        store(ptr.data());
    }

    void store(const PersistentObject *object);

    void storeString(const QString &t);
    QString loadString(int id);
    QString idLoadString();

    void storeStringSet(const QSet<QString> &t);
    QSet<QString> loadStringSet(const QList<int> &id);
    QSet<QString> idLoadStringSet();

    void storeStringList(const QStringList &t);
    QStringList loadStringList(const QList<int> &ids);
    QStringList idLoadStringList();

    const HeadData &headData() const { return m_headData; }
    void setHeadData(const HeadData &hd) { m_headData = hd; }

private:
    QDataStream m_stream;
    HeadData m_headData;
    QVector<PersistentObject *> m_loadedRaw;
    QVector<QSharedPointer<PersistentObject> > m_loaded;
    QHash<const PersistentObject *, int> m_storageIndices;
    PersistentObjectId m_lastStoredObjectId;

    QVector<QString> m_stringStorage;
    QHash<QString, int> m_inverseStringStorage;
    PersistentObjectId m_lastStoredStringId;
};

template<typename T> struct RemovePointer { typedef T Type; };
template<typename T> struct RemovePointer<T*> { typedef T Type; };
template <class T> struct RemoveConst { typedef T Type; };
template <class T> struct RemoveConst<const T> { typedef T Type; };

template <typename T>
void loadContainerS(T &container, PersistentPool &pool)
{
    int count;
    pool.stream() >> count;
    container.clear();
    container.reserve(count);
    for (int i = count; --i >= 0;)
        container += pool.idLoadS<typename RemoveConst<typename T::value_type::value_type>::Type>();
}

template <typename T>
void loadContainer(T &container, PersistentPool &pool)
{
    int count;
    pool.stream() >> count;
    container.clear();
    container.reserve(count);
    for (int i = count; --i >= 0;)
        container += pool.idLoad<typename RemovePointer<typename T::value_type>::Type>();
}

template <typename T>
void storeContainer(T &container, PersistentPool &pool)
{
    pool.stream() << container.count();
    typename T::const_iterator it = container.constBegin();
    const typename T::const_iterator itEnd = container.constEnd();
    for (; it != itEnd; ++it)
        pool.store(*it);
}

} // namespace qbs

#endif
