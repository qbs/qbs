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

class RulesApplicator
{
public:
    RulesApplicator(const ResolvedProductPtr &product, const Logger &logger);
    ~RulesApplicator();

    void applyRuleInEvaluationContext(const RuleConstPtr &rule,
            const ArtifactSet &inputArtifacts);
    const NodeSet &createdArtifacts() const { return m_createdArtifacts; }
    const NodeSet &invalidatedArtifacts() const { return m_invalidatedArtifacts; }

    void applyRule(const RuleConstPtr &rule, const ArtifactSet &inputArtifacts);
    static void handleRemovedRuleOutputs(const ArtifactSet &inputArtifacts,
            ArtifactSet artifactsToRemove, const Logger &logger);

private:
    void doApply(const ArtifactSet &inputArtifacts, QScriptValue &prepareScriptContext);
    ArtifactSet collectOldOutputArtifacts(const ArtifactSet &inputArtifacts) const;
    Artifact *createOutputArtifactFromRuleArtifact(const RuleArtifactConstPtr &ruleArtifact,
            const ArtifactSet &inputArtifacts, QSet<QString> *outputFilePaths);
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
    NodeSet m_createdArtifacts;
    NodeSet m_invalidatedArtifacts;
    RuleConstPtr m_rule;
    ArtifactSet m_completeInputSet;
    TransformerPtr m_transformer;
    QtMocScanner *m_mocScanner;
    Logger m_logger;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_RULESAPPLICATOR_H
