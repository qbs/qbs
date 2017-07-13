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

#ifndef QBS_MODULEPROPERTIES_H
#define QBS_MODULEPROPERTIES_H

#include <buildgraph/forward_decls.h>
#include <language/forward_decls.h>

#include <QtScript/qscriptcontext.h>
#include <QtScript/qscriptvalue.h>

namespace qbs {
namespace Internal {

class ScriptEngine;

enum ModulePropertiesScriptValueCommonPropertyKeys : quint32
{
    ModuleNameKey,
    ProductPtrKey,
    ArtifactPtrKey,
    DependencyParametersKey,
};

QScriptValue getDataForModuleScriptValue(QScriptEngine *engine, const ResolvedProduct *product,
                                         const Artifact *artifact, const ResolvedModule *module);

class ModuleProperties
{
public:
    static void init(QScriptValue productObject, const ResolvedProduct *product);
    static void init(QScriptValue artifactObject, const Artifact *artifact);
    static void setModuleScriptValue(QScriptValue targetObject, const QScriptValue &moduleObject,
                                     const QString &moduleName);

private:
    static void init(QScriptValue objectWithProperties, const void *ptr, const QString &type);
    static void setupModules(QScriptValue &object, const ResolvedProduct *product,
                             const Artifact *artifact);

    static QScriptValue js_moduleProperty(QScriptContext *context, QScriptEngine *engine);

    static QScriptValue moduleProperty(QScriptContext *context, QScriptEngine *engine);
};

} // namespace Internal
} // namespace qbs

#endif // QBS_MODULEPROPERTIES_H
