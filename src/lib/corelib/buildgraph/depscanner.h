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

#ifndef QBS_DEPENDENCY_SCANNER_H
#define QBS_DEPENDENCY_SCANNER_H

#include <language/forward_decls.h>
#include <language/filetags.h>
#include <language/preparescriptobserver.h>

#include <QStringList>
#include <QScriptValue>

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
    virtual ~DependencyScanner() {}

    virtual QStringList collectSearchPaths(Artifact *artifact) = 0;
    virtual QStringList collectDependencies(FileResourceBase *file) = 0;
    virtual bool recursive() const = 0;
    virtual const void *key() const = 0;
};

class PluginDependencyScanner : public DependencyScanner
{
public:
    PluginDependencyScanner(ScannerPlugin *plugin);

private:
    QStringList collectSearchPaths(Artifact *artifact);
    QStringList collectDependencies(FileResourceBase *file);
    bool recursive() const;
    const void *key() const;

    ScannerPlugin* m_plugin;
};

class UserDependencyScanner : public DependencyScanner
{
public:
    UserDependencyScanner(const ResolvedScannerConstPtr &scanner, const Logger &logger);
    ~UserDependencyScanner();

private:
    QStringList collectSearchPaths(Artifact *artifact);
    QStringList collectDependencies(FileResourceBase *file);
    bool recursive() const;
    const void *key() const;

    QStringList evaluate(Artifact *artifact, const ScriptFunctionPtr &script);

    ResolvedScannerConstPtr m_scanner;
    Logger m_logger;
    ScriptEngine *m_engine;
    PrepareScriptObserver m_observer;
    QScriptValue m_global;
    void *m_product;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_DEPENDENCY_SCANNER_H
