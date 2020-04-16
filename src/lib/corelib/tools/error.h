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

#ifndef QBS_ERROR
#define QBS_ERROR

#include "codelocation.h"

#include <QtCore/qhash.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qshareddata.h>

QT_BEGIN_NAMESPACE
class QJsonObject;
template <class T> class QList;
class QString;
class QStringList;
QT_END_NAMESPACE

namespace qbs {
namespace Internal { class PersistentPool; }
class CodeLocation;

class QBS_EXPORT ErrorItem
{
    friend class ErrorInfo;
public:
    ErrorItem();
    ErrorItem(const ErrorItem &rhs);
    ErrorItem(ErrorItem &&rhs) noexcept;
    ErrorItem &operator=(const ErrorItem &other);
    ErrorItem &operator=(ErrorItem &&other) noexcept;
    ~ErrorItem();

    QString description() const;
    CodeLocation codeLocation() const;
    QString toString() const;
    QJsonObject toJson() const;

    bool isBacktraceItem() const;

    void load(Internal::PersistentPool &pool);
    void store(Internal::PersistentPool &pool) const;

private:
    ErrorItem(const QString &description, const CodeLocation &codeLocation,
              bool isBacktraceItem = false);

    class ErrorItemPrivate;
    QExplicitlySharedDataPointer<ErrorItemPrivate> d;
};

class QBS_EXPORT ErrorInfo
{
public:
    ErrorInfo();
    ErrorInfo(const ErrorInfo &rhs);
    ErrorInfo(ErrorInfo &&rhs) noexcept;
    ErrorInfo(const QString &description, const CodeLocation &location = CodeLocation(),
              bool internalError = false);
    ErrorInfo(const QString &description, const QStringList &backtrace);
    ErrorInfo &operator=(const ErrorInfo &other);
    ErrorInfo &operator=(ErrorInfo &&other) noexcept;
    ~ErrorInfo();

    void appendBacktrace(const QString &description, const CodeLocation &location = CodeLocation());
    void append(const ErrorItem &item);
    void append(const QString &description, const CodeLocation &location = CodeLocation());
    void prepend(const QString &description, const CodeLocation &location = CodeLocation());
    const QList<ErrorItem> items() const;
    bool hasError() const { return !items().empty(); }
    void clear();
    QString toString() const;
    QJsonObject toJson() const;
    bool isInternalError() const;
    bool hasLocation() const;

    void load(Internal::PersistentPool &pool);
    void store(Internal::PersistentPool &pool) const;

private:
    class ErrorInfoPrivate;
    QSharedDataPointer<ErrorInfoPrivate> d;
};

void appendError(ErrorInfo &dst, const ErrorInfo &src);
inline uint qHash(const ErrorInfo &e) { return qHash(e.toString()); }
inline bool operator==(const ErrorInfo &e1, const ErrorInfo &e2) {
    return e1.toString() == e2.toString();
}

} // namespace qbs

Q_DECLARE_METATYPE(qbs::ErrorInfo)

#endif // QBS_ERROR
