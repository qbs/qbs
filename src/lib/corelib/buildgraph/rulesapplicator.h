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
#ifndef QBS_RULESAPPLICATOR_H
#define QBS_RULESAPPLICATOR_H

#include "artifactset.h"
#include "forward_decls.h"
#include "nodeset.h"
#include <language/filetags.h>
#include <language/forward_decls.h>
#include <logging/logger.h>

#include <QHash>
#include <QScriptValue>
#include <QString>

namespace qbs {
namespace Internal {
class BuildGraphNode;
class QtMocScanner;
class ScriptEngine;

typedef QHash<FileTag, ArtifactSet> ArtifactsPerFileTagMap;

class RulesApplicator
{
public:
    RulesApplicator(const ResolvedProductPtr &product, ArtifactsPerFileTagMap &artifactsPerFileTag,
                    const Logger &logger);
    ~RulesApplicator();
    NodeSet applyRuleInEvaluationContext(const RuleConstPtr &rule);
    void applyRule(const RuleConstPtr &rule);
    static void handleRemovedRuleOutputs(ArtifactSet artifactsToRemove, const Logger &logger);

private:
    void doApply(ArtifactSet inputArtifacts, QScriptValue &prepareScriptContext);
    void setupScriptEngineForArtifact(Artifact *artifact);
    ArtifactSet collectOldOutputArtifacts(const ArtifactSet &inputArtifacts) const;
    Artifact *createOutputArtifactFromRuleArtifact(const RuleArtifactConstPtr &ruleArtifact,
            const ArtifactSet &inputArtifacts);
    Artifact *createOutputArtifact(const QString &filePath, const FileTags &fileTags,
            bool alwaysUpdated, const ArtifactSet &inputArtifacts);
    QList<Artifact *> runOutputArtifactsScript(const ArtifactSet &inputArtifacts,
            const QScriptValueList &args);
    Artifact *createOutputArtifactFromScriptValue(const QScriptValue &obj,
            const ArtifactSet &inputArtifacts);
    QString resolveOutPath(const QString &path) const;
    RulesEvaluationContextPtr evalContext() const;
    ScriptEngine *engine() const;
    QScriptValue scope() const;

    const ResolvedProductPtr m_product;
    ArtifactsPerFileTagMap &m_artifactsPerFileTag;
    NodeSet m_createdArtifacts;

    RuleConstPtr m_rule;
    TransformerPtr m_transformer;
    QtMocScanner *m_mocScanner;
    Logger m_logger;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_RULESAPPLICATOR_H
