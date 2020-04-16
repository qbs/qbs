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

#ifndef QBS_DEPENDENCY_SCANNER_H
#define QBS_DEPENDENCY_SCANNER_H

#include <language/forward_decls.h>
#include <language/filetags.h>
#include <language/preparescriptobserver.h>

#include <QtCore/qstringlist.h>

#include <QtScript/qscriptvalue.h>

class ScannerPlugin;

namespace qbs {
namespace Internal {

class Artifact;
class FileResourceBase;
class Logger;
class ScriptEngine;

class DependencyScanner
{
public:
    virtual ~DependencyScanner() = default;

    QString id() const;

    virtual QStringList collectSearchPaths(Artifact *artifact) = 0;
    virtual QStringList collectDependencies(Artifact *artifact, FileResourceBase *file,
                                            const char *fileTags) = 0;
    virtual bool recursive() const = 0;
    virtual const void *key() const = 0;
    virtual bool areModulePropertiesCompatible(const PropertyMapConstPtr &m1,
                                               const PropertyMapConstPtr &m2) const = 0;
    virtual bool cacheIsPerFile() const = 0;

private:
    virtual QString createId() const = 0;

    mutable QString m_id;
};

class PluginDependencyScanner : public DependencyScanner
{
public:
    PluginDependencyScanner(ScannerPlugin *plugin);

private:
    QStringList collectSearchPaths(Artifact *artifact) override;
    QStringList collectDependencies(Artifact *artifact, FileResourceBase *file,
                                    const char *fileTags) override;
    bool recursive() const override;
    const void *key() const override;
    QString createId() const override;
    bool areModulePropertiesCompatible(const PropertyMapConstPtr &m1,
                                       const PropertyMapConstPtr &m2) const override;
    bool cacheIsPerFile() const override { return false; }

    ScannerPlugin* m_plugin;
};

class UserDependencyScanner : public DependencyScanner
{
public:
    UserDependencyScanner(ResolvedScannerConstPtr scanner, ScriptEngine *engine);

private:
    QStringList collectSearchPaths(Artifact *artifact) override;
    QStringList collectDependencies(Artifact *artifact, FileResourceBase *file,
                                    const char *fileTags) override;
    bool recursive() const override;
    const void *key() const override;
    QString createId() const override;
    bool areModulePropertiesCompatible(const PropertyMapConstPtr &m1,
                                       const PropertyMapConstPtr &m2) const override;
    bool cacheIsPerFile() const override { return true; }

    QStringList evaluate(const Artifact *artifact, const FileResourceBase *fileToScan, const PrivateScriptFunction &script);

    ResolvedScannerConstPtr m_scanner;
    ScriptEngine *m_engine;
    QScriptValue m_global;
    ResolvedProduct *m_product;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_DEPENDENCY_SCANNER_H
