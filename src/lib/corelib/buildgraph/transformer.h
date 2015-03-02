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

#ifndef QBS_TRANSFORMER_H
#define QBS_TRANSFORMER_H

#include "artifactset.h"
#include "forward_decls.h"
#include <language/forward_decls.h>
#include <language/property.h>
#include <tools/persistentobject.h>

#include <QHash>
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

    ArtifactSet inputs; // Subset of "children of all outputs".
    ArtifactSet outputs;
    RuleConstPtr rule;
    QList<AbstractCommandPtr> commands;
    PropertySet propertiesRequestedInPrepareScript;
    PropertySet propertiesRequestedInCommands;
    QHash<QString, PropertySet> propertiesRequestedFromArtifactInPrepareScript;

    static QScriptValue translateFileConfig(QScriptEngine *scriptEngine,
                                            Artifact *artifact,
                                            const QString &defaultModuleName);
    static QScriptValue translateInOutputs(QScriptEngine *scriptEngine,
                                           const ArtifactSet &artifacts,
                                           const QString &defaultModuleName);

    ResolvedProductPtr product() const;
    static void setupInputs(QScriptValue targetScriptValue, const ArtifactSet &inputs,
            const QString &defaultModuleName);
    void setupInputs(QScriptValue targetScriptValue);
    void setupOutputs(QScriptEngine *scriptEngine, QScriptValue targetScriptValue);
    void createCommands(const ScriptFunctionConstPtr &script,
                        const RulesEvaluationContextPtr &evalContext, const QScriptValueList &args);

private:
    Transformer();

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_TRANSFORMER_H
