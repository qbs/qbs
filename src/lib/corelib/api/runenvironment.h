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

#ifndef QBS_RUNENVIRONMENT_H
#define QBS_RUNENVIRONMENT_H

#include <language/forward_decls.h>
#include <tools/qbs_export.h>

#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE
class QProcess;
class QProcessEnvironment;
class QString;
class QStringList;
QT_END_NAMESPACE

class TestApi;

namespace qbs {
class ErrorInfo;
class InstallOptions;
class Settings;

namespace Internal {
class Logger;
class ResolvedProduct;
} // namespace Internal

class QBS_EXPORT RunEnvironment
{
    friend class CommandLineFrontend;
    friend class Project;
    friend class ::TestApi;
public:
    ~RunEnvironment();

    int runShell(ErrorInfo *error = nullptr);
    int runTarget(const QString &targetBin, const QStringList &arguments, bool dryRun,
                  ErrorInfo *error = nullptr);

    const QProcessEnvironment runEnvironment(ErrorInfo *error = nullptr) const;
    const QProcessEnvironment buildEnvironment(ErrorInfo *error = nullptr) const;

private:
    RunEnvironment(const Internal::ResolvedProductPtr &product,
                   const Internal::TopLevelProjectConstPtr &project,
                   const InstallOptions &installOptions,
                   const QProcessEnvironment &environment,
                   const QStringList &setupRunEnvConfig,
                   Settings *settings,
                   const Internal::Logger &logger);

    int doRunShell();
    int doRunTarget(const QString &targetBin, const QStringList &arguments, bool dryRun);
    void printStartInfo(const QProcess &proc, bool dryRun);

    const QProcessEnvironment getRunEnvironment() const;
    const QProcessEnvironment getBuildEnvironment() const;

    class RunEnvironmentPrivate;
    RunEnvironmentPrivate * const d;
};

} // namespace qbs

#endif // QBS_RUNENVIRONMENT_H
