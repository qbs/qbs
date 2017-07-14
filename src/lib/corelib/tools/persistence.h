/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QBS_PERSISTENCE
#define QBS_PERSISTENCE

#include "error.h"
#include "persistentobject.h"
#include <logging/logger.h>

#include <QtCore/qdatastream.h>
#include <QtCore/qflags.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

#include <memory>
#include <type_traits>
#include <vector>

namespace qbs {
namespace Internal {

class NoBuildGraphError : public ErrorInfo
{
public:
    NoBuildGraphError();
};

class PersistentPool
{
public:
    PersistentPool(Logger &logger);
    ~PersistentPool();

    class HeadData
    {
    public:
        QVariantMap projectConfig;
    };

    // We need a helper class template, because we require partial specialization for some of
    // the aggregate types, which is not possible with function templates.
    // The generic implementation assumes that T is of class type and has load() and store()
    // member functions.
    template<typename T, typename Enable = void> struct Helper
    {
        static void store(const T &object, PersistentPool *pool) { object.store(*pool); }
        static void load(T &object, PersistentPool *pool) { object.load(*pool); }
    };

    template<typename T> void store(const T &value) { Helper<T>().store(value, this); }
    template<typename T> void load(T &value) { Helper<T>().load(value, this); }
    template<typename T> T load() {
        T tmp;
        Helper<T>().load(tmp, this);
        return tmp;
    }

    void load(const QString &filePath);
    void setupWriteStream(const QString &filePath);
    void finalizeWriteStream();
    void closeStream();
    void clear();

    const HeadData &headData() const { return m_headData; }
    void setHeadData(const HeadData &hd) { m_headData = hd; }

private:
    typedef int PersistentObjectId;

    template <typename T> T *idLoad();
    template <class T> std::shared_ptr<T> idLoadS();

    void storePersistentObject(const PersistentObject *object);

    void storeVariant(const QVariant &variant);
    QVariant loadVariant();

    void storeString(const QString &t);
    QString loadString(int id);
    QString idLoadString();

    QDataStream m_stream;
    HeadData m_headData;
    std::vector<PersistentObject *> m_loadedRaw;
    std::vector<std::shared_ptr<PersistentObject> > m_loaded;
    QHash<const PersistentObject *, int> m_storageIndices;
    PersistentObjectId m_lastStoredObjectId;

    std::vector<QString> m_stringStorage;
    QHash<QString, int> m_inverseStringStorage;
    PersistentObjectId m_lastStoredStringId;
    Logger &m_logger;
};

template <typename T> inline T *PersistentPool::idLoad()
{
    PersistentObjectId id;
    m_stream >> id;

    if (id < 0)
        return 0;

    if (id < static_cast<PersistentObjectId>(m_loadedRaw.size())) {
        PersistentObject *obj = m_loadedRaw.at(id);
        return dynamic_cast<T*>(obj);
    }

    auto i = m_loadedRaw.size();
    m_loadedRaw.resize(id + 1);
    for (; i < m_loadedRaw.size(); ++i)
        m_loadedRaw[i] = 0;

    T * const t = new T;
    PersistentObject * const po = t;
    m_loadedRaw[id] = po;
    po->load(*this);
    return t;
}

template <class T> inline std::shared_ptr<T> PersistentPool::idLoadS()
{
    PersistentObjectId id;
    m_stream >> id;

    if (id < 0)
        return std::shared_ptr<T>();

    if (id < static_cast<PersistentObjectId>(m_loaded.size())) {
        std::shared_ptr<PersistentObject> obj = m_loaded.at(id);
        return std::dynamic_pointer_cast<T>(obj);
    }

    m_loaded.resize(id + 1);
    const std::shared_ptr<T> t = T::create();
    m_loaded[id] = t;
    PersistentObject * const po = t.get();
    po->load(*this);
    return t;
}

/***** Specializations of Helper class *****/

template<typename T>
struct PersistentPool::Helper<T, typename std::enable_if<std::is_integral<T>::value>::type>
{
    static void store(const T &value, PersistentPool *pool) { pool->m_stream << value; }
    static void load(T &value, PersistentPool *pool) { pool->m_stream >> value; }
};

template<typename T>
struct PersistentPool::Helper<T, typename std::enable_if<std::is_enum<T>::value>::type>
{
    using U = typename std::underlying_type<T>::type;
    static void store(const T &value, PersistentPool *pool)
    {
        pool->m_stream << static_cast<U>(value);
    }
    static void load(T &value, PersistentPool *pool)
    {
        pool->m_stream >> reinterpret_cast<U &>(value);
    }
};

// TODO: Use constexpr function once we require MSVC 2015.
template<typename T> struct IsPersistentObject
{
    static const bool value = std::is_base_of<PersistentObject, T>::value;
};

template<typename T>
struct PersistentPool::Helper<std::shared_ptr<T>,
                              typename std::enable_if<IsPersistentObject<T>::value>::type>
{
    static void store(const std::shared_ptr<T> &value, PersistentPool *pool)
    {
        pool->store(value.get());
    }
    static void load(std::shared_ptr<T> &value, PersistentPool *pool)
    {
        value = pool->idLoadS<typename std::remove_const<T>::type>();
    }
};

template<typename T>
struct PersistentPool::Helper<T *, typename std::enable_if<IsPersistentObject<T>::value>::type>
{
    static void store(const T *value, PersistentPool *pool) { pool->storePersistentObject(value); }
    void load(T* &value, PersistentPool *pool) { value = pool->idLoad<T>(); }
};

template<> struct PersistentPool::Helper<QString>
{
    static void store(const QString &s, PersistentPool *pool) { pool->storeString(s); }
    static void load(QString &s, PersistentPool *pool) { s = pool->idLoadString(); }
};

template<> struct PersistentPool::Helper<QVariant>
{
    static void store(const QVariant &v, PersistentPool *pool) { pool->storeVariant(v); }
    static void load(QVariant &v, PersistentPool *pool) { v = pool->loadVariant(); }
};

template<> struct PersistentPool::Helper<QProcessEnvironment>
{
    static void store(const QProcessEnvironment &env, PersistentPool *pool)
    {
        const QStringList &keys = env.keys();
        pool->store(keys.count());
        for (const QString &key : keys) {
            pool->store(key);
            pool->store(env.value(key));
        }
    }
    static void load(QProcessEnvironment &env, PersistentPool *pool)
    {
        const int count = pool->load<int>();
        for (int i = 0; i < count; ++i) {
            const auto &key = pool->load<QString>();
            const auto &value = pool->load<QString>();
            env.insert(key, value);
        }
    }
};
template<typename T, typename U> struct PersistentPool::Helper<std::pair<T, U>>
{
    static void store(const std::pair<T, U> &pair, PersistentPool *pool)
    {
        pool->store(pair.first);
        pool->store(pair.second);
    }
    static void load(std::pair<T, U> &pair, PersistentPool *pool)
    {
        pool->load(pair.first);
        pool->load(pair.second);
    }
};

template<typename T> struct PersistentPool::Helper<QFlags<T>>
{
    using Int = typename QFlags<T>::Int;
    static void store(const QFlags<T> &flags, PersistentPool *pool)
    {
        pool->store<Int>(flags);
    }
    static void load(QFlags<T> &flags, PersistentPool *pool)
    {
        flags = QFlags<T>(pool->load<Int>());
    }
};

template<typename T> struct IsSimpleContainer { static const bool value = false; };
template<> struct IsSimpleContainer<QStringList> { static const bool value = true; };
template<typename T> struct IsSimpleContainer<QList<T>> { static const bool value = true; };
template<typename T> struct IsSimpleContainer<std::vector<T>> { static const bool value = true; };

template<typename T>
struct PersistentPool::Helper<T, typename std::enable_if<IsSimpleContainer<T>::value>::type>
{
    static void store(const T &container, PersistentPool *pool)
    {
        pool->store(int(container.size()));
        for (auto it = container.cbegin(); it != container.cend(); ++it)
            pool->store(*it);
    }
    static void load(T &container, PersistentPool *pool)
    {
        const int count = pool->load<int>();
        container.clear();
        container.reserve(count);
        for (int i = count; --i >= 0;)
            container.push_back(pool->load<typename T::value_type>());
    }
};

template<typename T> struct IsKeyValueContainer { static const bool value = false; };
template<typename K, typename V> struct IsKeyValueContainer<QMap<K, V>>
{
    static const bool value = true;
};
template<typename K, typename V> struct IsKeyValueContainer<QHash<K, V>>
{
    static const bool value = true;
};

template<typename T>
struct PersistentPool::Helper<T, typename std::enable_if<IsKeyValueContainer<T>::value>::type>
{
    static void store(const T &container, PersistentPool *pool)
    {
        pool->store(container.count());
        for (auto it = container.cbegin(); it != container.cend(); ++it) {
            pool->store(it.key());
            pool->store(it.value());
        }
    }
    static void load(T &container, PersistentPool *pool)
    {
        container.clear();
        const int count = pool->load<int>();
        for (int i = 0; i < count; ++i) {
            const auto &key = pool->load<typename T::key_type>();
            const auto &value = pool->load<typename T::mapped_type>();
            container.insert(key, value);
        }
    }
};

} // namespace Internal
} // namespace qbs

#endif
