/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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
#ifndef QBS_CLEANOPTIONS_H
#define QBS_CLEANOPTIONS_H

#include "qbs_export.h"

#include <QSharedDataPointer>
#include <QString>

namespace qbs {
namespace Internal { class CleanOptionsPrivate; }

class QBS_EXPORT CleanOptions
{
public:
    CleanOptions();
    CleanOptions(const CleanOptions &other);
    CleanOptions &operator=(const CleanOptions &other);
    ~CleanOptions();

    enum CleanType { CleanupAll, CleanupTemporaries };
    CleanType cleanType() const;
    void setCleanType(CleanType cleanType);

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
