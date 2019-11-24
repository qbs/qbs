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
#ifndef QBS_RULESAPPLICATOR_H
#define QBS_RULESAPPLICATOR_H

#include "artifact.h"
#include "forward_decls.h"
#include "nodeset.h"
#include <language/filetags.h>
#include <language/forward_decls.h>
#include <logging/logger.h>

#include <QtCore/qflags.h>
#include <QtCore/qhash.h>
#include <QtCore/qstring.h>
#include <QtScript/qscriptvalue.h>

#include <unordered_map>

namespace qbs {
namespace Internal {
class BuildGraphNode;
class QtMocScanner;
class ScriptEngine;

class RulesApplicator
{
public:
    RulesApplicator(ResolvedProductPtr product,
                    const std::unordered_map<QString, const ResolvedProduct *> &productsByName,
                    const std::unordered_map<QString, const ResolvedProject *> &projectsByName,
                    Logger logger);
    ~RulesApplicator();

    const NodeSet &createdArtifacts() const { return m_createdArtifacts; }
    const NodeSet &invalidatedArtifacts() const { return m_invalidatedArtifacts; }
    QStringList removedArtifacts() const { return m_removedArtifacts; }
    bool ruleUsesIo() const { return m_ruleUsesIo; }

    void applyRule(RuleNode *ruleNode, const ArtifactSet &inputArtifacts,
                   const ArtifactSet &explicitlyDependsOn);
    static void handleRemovedRuleOutputs(const ArtifactSet &inputArtifacts,
            const ArtifactSet &artifactsToRemove, QStringList &removedArtifacts,
            const Logger &logger);
    static ArtifactSet collectAuxiliaryInputs(const Rule *rule, const ResolvedProduct *product);
    static ArtifactSet collectExplicitlyDependsOn(const Rule *rule, const ResolvedProduct *product);

    enum InputsSourceFlag { CurrentProduct = 1, Dependencies = 2 };
    Q_DECLARE_FLAGS(InputsSources, InputsSourceFlag)

private:
    void doApply(const ArtifactSet &inputArtifacts, QScriptValue &prepareScriptContext);
    ArtifactSet collectOldOutputArtifacts(const ArtifactSet &inputArtifacts) const;

    struct OutputArtifactInfo {
        Artifact *artifact = nullptr;
        bool newlyCreated = false;
        FileTags oldFileTags;
        QVariantMap oldProperties;
    };
    OutputArtifactInfo createOutputArtifactFromRuleArtifact(
            const RuleArtifactConstPtr &ruleArtifact, const ArtifactSet &inputArtifacts,
            Set<QString> *outputFilePaths);
    OutputArtifactInfo createOutputArtifact(const QString &filePath, const FileTags &fileTags,
            bool alwaysUpdated, const ArtifactSet &inputArtifacts);
    QList<Artifact *> runOutputArtifactsScript(const ArtifactSet &inputArtifacts,
            const QScriptValueList &args);
    Artifact *createOutputArtifactFromScriptValue(const QScriptValue &obj,
            const ArtifactSet &inputArtifacts);
    QString resolveOutPath(const QString &path) const;
    const RulesEvaluationContextPtr &evalContext() const;
    ScriptEngine *engine() const;
    QScriptValue scope() const;

    static ArtifactSet collectAdditionalInputs(const FileTags &tags,
                                               const Rule *rule, const ResolvedProduct *product,
                                               InputsSources inputsSources);

    const ResolvedProductPtr m_product;
    const std::unordered_map<QString, const ResolvedProduct *> &m_productsByName;
    const std::unordered_map<QString, const ResolvedProject *> &m_projectsByName;
    ArtifactSet m_explicitlyDependsOn;
    NodeSet m_createdArtifacts;
    NodeSet m_invalidatedArtifacts;
    QStringList m_removedArtifacts;
    RuleNode *m_ruleNode = nullptr;
    RuleConstPtr m_rule;
    ArtifactSet m_completeInputSet;
    TransformerPtr m_transformer;
    TransformerConstPtr m_oldTransformer;
    QtMocScanner *m_mocScanner;
    Logger m_logger;
    bool m_ruleUsesIo = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(RulesApplicator::InputsSources)

} // namespace Internal
} // namespace qbs

#endif // QBS_RULESAPPLICATOR_H
