/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#ifndef QBS_RUNENVIRONMENT_H
#define QBS_RUNENVIRONMENT_H

#include <language/forward_decls.h>
#include <tools/qbs_export.h>

#include <QStringList>

QT_BEGIN_NAMESPACE
class QProcessEnvironment;
QT_END_NAMESPACE

namespace qbs {
class InstallOptions;
class Settings;

namespace Internal {
class Logger;
class ResolvedProduct;
} // namespace Internal

class QBS_EXPORT RunEnvironment
{
    friend class Project;
public:
    ~RunEnvironment();

    // These can throw an Error
    int runShell();
    int runTarget(const QString &targetBin, const QStringList &arguments);

    const QProcessEnvironment runEnvironment() const;
    const QProcessEnvironment buildEnvironment() const;

private:
    RunEnvironment(const Internal::ResolvedProductPtr &product,
                   const InstallOptions &installOptions,
                   const QProcessEnvironment &environment, Settings *settings,
                   const Internal::Logger &logger);

    class RunEnvironmentPrivate;
    RunEnvironmentPrivate * const d;
};

} // namespace qbs

#endif // QBS_RUNENVIRONMENT_H
