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
#ifndef QBS_CLEANOPTIONS_H
#define QBS_CLEANOPTIONS_H

#include "qbs_export.h"

#include <QtCore/qshareddata.h>

QT_BEGIN_NAMESPACE
class QJsonObject;
QT_END_NAMESPACE

namespace qbs {
namespace Internal { class CleanOptionsPrivate; }

class QBS_EXPORT CleanOptions
{
public:
    CleanOptions();
    CleanOptions(const CleanOptions &other);
    CleanOptions(CleanOptions &&other) Q_DECL_NOEXCEPT;
    CleanOptions &operator=(const CleanOptions &other);
    CleanOptions &operator=(CleanOptions &&other) Q_DECL_NOEXCEPT;
    ~CleanOptions();

    static CleanOptions fromJson(const QJsonObject &data);

    bool dryRun() const;
    void setDryRun(bool dryRun);

    bool keepGoing() const;
    void setKeepGoing(bool keepGoing);

    bool logElapsedTime() const;
    void setLogElapsedTime(bool log);

private:
    QSharedDataPointer<Internal::CleanOptionsPrivate> d;
};

} // namespace qbs

#endif // Include guard
