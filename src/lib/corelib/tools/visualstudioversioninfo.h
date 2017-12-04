/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2015 Jake Petroules.
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

#ifndef QBS_VISUALSTUDIOVERSIONINFO_H
#define QBS_VISUALSTUDIOVERSIONINFO_H

#include "qbs_export.h"

#include <tools/version.h>

#include <QtCore/qstring.h>

#include <set>

namespace qbs {
namespace Internal {

class QBS_EXPORT VisualStudioVersionInfo
{
public:
    VisualStudioVersionInfo();
    VisualStudioVersionInfo(const Version &version);

    static std::set<VisualStudioVersionInfo> knownVersions();

    bool operator<(const VisualStudioVersionInfo &other) const;
    bool operator==(const VisualStudioVersionInfo &other) const;

    bool usesMsBuild() const;
    bool usesVcBuild() const;
    bool usesSolutions() const;

    Version version() const;
    int marketingVersion() const;

    QString solutionVersion() const;
    QString toolsVersion() const;
    QString platformToolsetVersion() const;

private:
    Version m_version;
};

quint32 qHash(const VisualStudioVersionInfo &info);

} // namespace Internal
} // namespace qbs

#endif // QBS_VISUALSTUDIOVERSIONINFO_H
