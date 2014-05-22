/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
    virtual const FileTags &fileTags() = 0;
    virtual bool recursive() = 0;
    virtual void *key() = 0;
};

class PluginDependencyScanner : public DependencyScanner
{
public:
    PluginDependencyScanner(ScannerPlugin *plugin);
    virtual QStringList collectSearchPaths(Artifact *artifact);
    virtual QStringList collectDependencies(FileResourceBase *file);
    virtual const FileTags &fileTags();
    virtual bool recursive();
    virtual void *key();
private:
    ScannerPlugin* m_plugin;
    FileTags m_fileTag;
};

class UserDependencyScanner : public DependencyScanner
{
public:
    UserDependencyScanner(const ResolvedScannerConstPtr &scanner, const Logger &logger,
                          ScriptEngine *engine);
    virtual QStringList collectSearchPaths(Artifact *artifact);
    virtual QStringList collectDependencies(FileResourceBase *file);
    virtual const FileTags &fileTags();
    virtual bool recursive();
    virtual void *key();
private:
    QStringList evaluate(Artifact *artifact, const ScriptFunctionPtr &script);
private:
    ResolvedScannerConstPtr m_scanner;
    Logger m_logger;
    ScriptEngine *m_engine;
    PrepareScriptObserver m_observer;
    QScriptValue m_global;
    void *m_project;
    void *m_product;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_DEPENDENCY_SCANNER_H
