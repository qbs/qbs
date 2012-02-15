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

#ifndef QBS_PERSISTENCE
#define QBS_PERSISTENCE

#include <QtCore/QString>
#include <QtCore/QSharedPointer>
#include <QtCore/QFile>
#include <QtCore/QDataStream>
#include <QtCore/QMap>
#include <QtCore/QVariantMap>
#include <QtCore/QDebug>
#include <tools/error.h>

namespace qbs {

typedef int PersistentObjectId;
typedef QByteArray PersistentObjectData;
class PersistentPool;

class PersistentObject
{
public:
    virtual ~PersistentObject() {}
    virtual void load(PersistentPool &/*pool*/, PersistentObjectData &/*data*/) {}
    virtual void store(PersistentPool &/*pool*/, PersistentObjectData &/*data*/) const {}
};

class PersistentPool
{
public:
    PersistentPool();
    ~PersistentPool();

    struct HeadData
    {
        QVariantMap projectConfig;
    };

    enum LoadMode
    {
        LoadHeadData, LoadAll
    };

    bool load(const QString &filePath, const LoadMode loadMode = LoadAll);
    bool store(const QString &filePath);
    void clear();

    template <typename T>
    inline T *idLoad(QDataStream &s)
    {
        PersistentObjectId id;
        s >> id;
        return loadRaw<T>(id);
    }

    template <class T>
    inline T *loadRaw(PersistentObjectId id)
    {
        if (id == 0)
            return 0;

        PersistentObject *obj = 0;
        if (id < m_loadedRaw.count()) {
            obj = m_loadedRaw.value(id);
        } else {
            int i = m_loadedRaw.count();
            m_loadedRaw.resize(id + 1);
            for (; i < m_loadedRaw.count(); ++i)
                m_loadedRaw[i] = 0;
        }
        if (!obj) {
            if (id >= m_storage.count())
                throw Error(QString("storage error: no id %1 stored.").arg(id).arg(m_storage.count()));
            PersistentObjectData data = m_storage.at(id);
            obj = new T;
            m_loadedRaw[id] = obj;
            obj->load(*this, data);
        }

        return static_cast<T*>(obj);
    }

    template <class T>
    inline typename T::Ptr idLoadS(QDataStream &s)
    {
        PersistentObjectId id;
        s >> id;
        return load<T>(id);
    }

    template <class T>
    inline QSharedPointer<T> load(PersistentObjectId id)
    {
        if (id == 0)
            return QSharedPointer<T>();

        QSharedPointer<PersistentObject> obj;
        if (id < m_loaded.count())
            obj = m_loaded.value(id);
        else
            m_loaded.resize(id + 1);
        if (!obj) {
            if (id >= m_storage.count())
                throw Error(QString("storage error: no id %1 stored. I have that many: %2").arg(id).arg(m_storage.count()));

            PersistentObjectData data = m_storage.at(id);
            T *t = new T;
            obj = QSharedPointer<PersistentObject>(t);
            m_loaded[id] = obj;
            obj->load(*this, data);
        }

        return obj.staticCast<T>();
    }

    PersistentObject *load(PersistentObjectId);

    template <class T>
    PersistentObjectId store(QSharedPointer<T> ptr)
    {
        return store(ptr.data());
    }

    PersistentObjectId store(PersistentObject *object);

    int storeString(const QString &t);
    QString loadString(int id);
    QString idLoadString(QDataStream &s);

    QList<int> storeStringSet(const QSet<QString> &t);
    QSet<QString> loadStringSet(const QList<int> &id);
    QSet<QString> idLoadStringSet(QDataStream &s);

    QList<int> storeStringList(const QStringList &t);
    QStringList loadStringList(const QList<int> &ids);
    QStringList idLoadStringList(QDataStream &s);

    PersistentObjectData getData(PersistentObjectId) const;
    void setData(PersistentObjectId, PersistentObjectData);

    const HeadData &headData() const { return m_headData; }
    void setHeadData(const HeadData &hd) { m_headData = hd; }

private:
    static const int m_maxReservedId = 0;
    HeadData m_headData;
    QVector<PersistentObject *> m_loadedRaw;
    QVector<QSharedPointer<PersistentObject> > m_loaded;
    QVector<PersistentObjectData> m_storage;
    QHash<PersistentObject *, int> m_storageIndices;

    QVector<QString> m_stringStorage;
    QHash<QString, int> m_inverseStringStorage;
};

template<typename T> struct RemovePointer { typedef T Type; };
template<typename T> struct RemovePointer<T*> { typedef T Type; };

template <typename T>
void loadContainerS(T &container, QDataStream &s, qbs::PersistentPool &pool)
{
    int count;
    s >> count;
    container.clear();
    container.reserve(count);
    for (int i = count; --i >= 0;)
        container += pool.idLoadS<typename T::value_type::Type>(s);
}

template <typename T>
void loadContainer(T &container, QDataStream &s, qbs::PersistentPool &pool)
{
    int count;
    s >> count;
    container.clear();
    container.reserve(count);
    for (int i = count; --i >= 0;)
        container += pool.idLoad<typename RemovePointer<typename T::value_type>::Type>(s);
}

template <typename T>
void storeContainer(T &container, QDataStream &s, qbs::PersistentPool &pool)
{
    s << container.count();
    typename T::const_iterator it = container.constBegin();
    const typename T::const_iterator itEnd = container.constEnd();
    for (; it != itEnd; ++it)
        s << pool.store(*it);
}

template <typename T>
void storeHashContainer(T &container, QDataStream &s, qbs::PersistentPool &pool)
{
    s << container.count();
    foreach (const typename T::mapped_type &item, container)
        s << pool.store(item);
}

} // namespace qbs

#endif
