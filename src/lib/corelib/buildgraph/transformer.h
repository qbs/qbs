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

#ifndef QBS_TRANSFORMER_H
#define QBS_TRANSFORMER_H

#include "artifact.h"
#include "forward_decls.h"
#include <language/forward_decls.h>
#include <language/property.h>
#include <language/scriptengine.h>
#include <tools/persistentobject.h>

#include <QtCore/qhash.h>

namespace qbs {
namespace Internal {
class Artifact;
class AbstractCommand;
class Rule;

class Transformer : public PersistentObject
{
public:
    static TransformerPtr create() { return TransformerPtr(new Transformer); }

    ~Transformer();

    ArtifactSet inputs; // Subset of "children of all outputs".
    ArtifactSet outputs;
    ArtifactSet explicitlyDependsOn;
    RuleConstPtr rule;
    QList<AbstractCommandPtr> commands;
    PropertySet propertiesRequestedInPrepareScript;
    PropertySet propertiesRequestedInCommands;
    QHash<QString, PropertySet> propertiesRequestedFromArtifactInPrepareScript;
    QHash<QString, PropertySet> propertiesRequestedFromArtifactInCommands;
    bool alwaysRun;

    static QScriptValue translateFileConfig(ScriptEngine *scriptEngine,
                                            const Artifact *artifact,
                                            const QString &defaultModuleName);
    ResolvedProductPtr product() const;
    void setupInputs(QScriptValue targetScriptValue);
    void setupOutputs(QScriptValue targetScriptValue);
    void setupExplicitlyDependsOn(QScriptValue targetScriptValue);
    void createCommands(ScriptEngine *engine, const ScriptFunctionConstPtr &script,
                        const QScriptValueList &args);
    void rescueChangeTrackingData(const TransformerConstPtr &other);

private:
    Transformer();

    static void setupInputs(QScriptValue targetScriptValue, const ArtifactSet &inputs,
            const QString &defaultModuleName);
    static QScriptValue translateInOutputs(ScriptEngine *scriptEngine,
                                           const ArtifactSet &artifacts,
                                           const QString &defaultModuleName);

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_TRANSFORMER_H
