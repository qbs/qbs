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

#include "id.h"
#include "qbsassert.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qhash.h>

#include <mutex>
#include <vector>

namespace qbs {
namespace Internal {

/*!
    \class qbs::Internal::Id

    \brief The class Id encapsulates an identifier that is unique
    within a specific running process, using the qbs library.

    \c{Id} is used as facility to identify objects of interest
    in a more typesafe and faster manner than a plain \c QString or
    \c QByteArray would provide.

    An id is internally represented as a 32 bit integer (its \c UID)
    and associated with a plain 7-bit-clean ASCII name used
    for display and persistency.

    This class is copied from Qt Creator.
*/

class StringHolder
{
public:
    StringHolder() = default;

    StringHolder(const char *s, int length)
        : n(length), str(s)
    {
        if (!n)
            length = n = qstrlen(s);
        h = 0;
        while (length--) {
            h = (h << 4) + *s++;
            h ^= (h & 0xf0000000) >> 23;
            h &= 0x0fffffff;
        }
    }
    int n = 0;
    const char *str = nullptr;
    uint h = 0;
};

static bool operator==(const StringHolder &sh1, const StringHolder &sh2)
{
    // sh.n is unlikely to discriminate better than the hash.
    return sh1.h == sh2.h && sh1.str && sh2.str && strcmp(sh1.str, sh2.str) == 0;
}


static uint qHash(const StringHolder &sh)
{
    return sh.h;
}

struct IdCache : public QHash<StringHolder, int>
{
#ifndef QBS_ALLOW_STATIC_LEAKS
    ~IdCache()
    {
        for (IdCache::iterator it = begin(); it != end(); ++it)
            delete[](const_cast<char *>(it.key().str));
    }
#endif
};


static int firstUnusedId = Id::IdsPerPlugin * Id::ReservedPlugins;

static QHash<int, StringHolder> stringFromId;
static IdCache idFromString;
static std::mutex mutex;

static QByteArray getStringFromId(int id)
{
    std::lock_guard<std::mutex> lock(mutex);
    return stringFromId.value(id).str;
}

static int theId(const char *str, int n = 0)
{
    QBS_ASSERT(str && *str, return 0);
    StringHolder sh(str, n);
    std::lock_guard<std::mutex> lock(mutex);
    int res = idFromString.value(sh, 0);
    if (res == 0) {
        res = firstUnusedId++;
        sh.str = qstrdup(sh.str);
        idFromString[sh] = res;
        stringFromId[res] = sh;
    }
    return res;
}

static int theId(const QByteArray &ba)
{
    return theId(ba.constData(), ba.size());
}

/*!
    \fn qbs::Internal::Id(int uid)

    \brief Constructs an id given a UID.

    The UID is an integer value that is unique within the running
    process.

    It is the callers responsibility to ensure the uniqueness of
    the passed integer. The recommended approach is to use
    \c{registerId()} with an value taken from the plugin's
    private range.

    \sa registerId()

*/

/*!
    Constructs an id given its associated name. The internal
    representation will be unspecified, but consistent within a
    process.

*/
Id::Id(const char *name)
    : m_id(theId(name, 0))
{}

/*!
    \overload

*/
Id::Id(const QByteArray &name)
   : m_id(theId(name))
{}

/*!
  Returns an internal representation of the id.
*/

QByteArray Id::name() const
{
    return getStringFromId(m_id);
}

/*!
  Returns a string representation of the id suitable
  for UI display.

  This should not be used to create a persistent version
  of the Id, use \c{toSetting()} instead.

  \sa fromString(), toSetting()
*/

QString Id::toString() const
{
    return QString::fromUtf8(getStringFromId(m_id));
}

/*!
  Returns a persistent value representing the id which is
  suitable to be stored in QSettings.

  \sa fromSetting()
*/

QVariant Id::toSetting() const
{
    return QString::fromUtf8(getStringFromId(m_id));
}

/*!
  Reconstructs an id from a persistent value.

  \sa toSetting()
*/

Id Id::fromSetting(const QVariant &variant)
{
    const QByteArray ba = variant.toString().toUtf8();
    if (ba.isEmpty())
        return {};
    return {theId(ba)};
}

/*!
  Constructs a derived id.

  This can be used to construct groups of ids logically
  belonging together. The associated internal name
  will be generated by appending \c{suffix}.
*/

Id Id::withSuffix(int suffix) const
{
    const QByteArray ba = name() + QByteArray::number(suffix);
    return {ba.constData()};
}

/*!
  \overload
*/

Id Id::withSuffix(const char *suffix) const
{
    const QByteArray ba = name() + suffix;
    return {ba.constData()};
}

/*!
  Constructs a derived id.

  This can be used to construct groups of ids logically
  belonging together. The associated internal name
  will be generated by prepending \c{prefix}.
*/

Id Id::withPrefix(const char *prefix) const
{
    const QByteArray ba = prefix + name();
    return {ba.constData()};
}

bool Id::operator==(const char *name) const
{
    const auto string = getStringFromId(m_id);
    if (!string.isNull() && name)
        return strcmp(string.data(), name) == 0;
    else
        return false;
}

bool Id::alphabeticallyBefore(Id other) const
{
    return toString().compare(other.toString(), Qt::CaseInsensitive) < 0;
}

} // namespace Internal
} // namespace qbs
