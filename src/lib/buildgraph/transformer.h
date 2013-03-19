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

#ifndef QBS_TRANSFORMER_H
#define QBS_TRANSFORMER_H

#include "artifactlist.h"
#include "forward_decls.h"
#include <language/forward_decls.h>
#include <language/property.h>
#include <tools/persistentobject.h>

#include <QScriptEngine>

namespace qbs {
namespace Internal {
class Artifact;
class AbstractCommand;
class Rule;
class ScriptEngine;

class Transformer : public PersistentObject
{
public:
    static TransformerPtr create() { return TransformerPtr(new Transformer); }

    ~Transformer();

    ArtifactList inputs; // can be different from "children of all outputs"
    ArtifactList outputs;
    RuleConstPtr rule;
    QList<AbstractCommand *> commands;
    PropertyList modulePropertiesUsedInPrepareScript;

    static QScriptValue translateFileConfig(QScriptEngine *scriptEngine,
                                            Artifact *artifact,
                                            const QString &defaultModuleName);
    static QScriptValue translateInOutputs(QScriptEngine *scriptEngine,
                                           const ArtifactList &artifacts,
                                           const QString &defaultModuleName);

    void setupInputs(QScriptEngine *scriptEngine, QScriptValue targetScriptValue);
    void setupOutputs(QScriptEngine *scriptEngine, QScriptValue targetScriptValue);
    void createCommands(const PrepareScriptConstPtr &script,
                        const RulesEvaluationContextPtr &evalContext);

private:
    Transformer();

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_TRANSFORMER_H
