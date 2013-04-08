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
#ifndef QBS_RULESAPPLICATOR_H
#define QBS_RULESAPPLICATOR_H

#include "artifactlist.h"
#include "forward_decls.h"
#include <language/filetags.h>
#include <language/forward_decls.h>
#include <language/scriptpropertyobserver.h>
#include <logging/logger.h>

#include <QMap>
#include <QScriptValue>
#include <QString>

namespace qbs {
namespace Internal {
class ScriptEngine;

typedef QMap<FileTag, ArtifactList> ArtifactsPerFileTagMap;

class RulesApplicator : private ScriptPropertyObserver
{
public:
    RulesApplicator(const ResolvedProductPtr &product, ArtifactsPerFileTagMap &artifactsPerFileTag,
                    const Logger &logger);
    void applyAllRules();
    void applyRule(const RuleConstPtr &rule);

private:
    void doApply(const ArtifactList &inputArtifacts);
    void setupScriptEngineForArtifact(Artifact *artifact);
    Artifact *createOutputArtifact(const RuleArtifactConstPtr &ruleArtifact,
                                   const ArtifactList &inputArtifacts);
    QString resolveOutPath(const QString &path) const;
    RulesEvaluationContextPtr evalContext() const;
    ScriptEngine *engine() const;
    QScriptValue scope() const;
    void onPropertyRead(const QScriptValue &object, const QString &name, const QScriptValue &value);

    const ResolvedProductPtr m_product;
    ArtifactsPerFileTagMap &m_artifactsPerFileTag;

    RuleConstPtr m_rule;
    TransformerPtr m_transformer;
    Logger m_logger;
    qint64 m_productObjectId;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_RULESAPPLICATOR_H
