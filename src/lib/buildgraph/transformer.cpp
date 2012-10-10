/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#include "transformer.h"
#include "artifact.h"
#include "command.h"

namespace qbs {

Transformer::Transformer()
{
}

Transformer::~Transformer()
{
    qDeleteAll(commands);
}

QScriptValue Transformer::translateFileConfig(QScriptEngine *scriptEngine, Artifact *artifact, const QString &defaultModuleName)
{
    QScriptValue config = artifact->configuration->toScriptValue(scriptEngine);
    QScriptValue artifactConfig = scriptEngine->newObject();
    artifactConfig.setPrototype(config);
    artifactConfig.setProperty(QLatin1String("fileName"), artifact->filePath());
    QStringList fileTags = artifact->fileTags.toList();
    artifactConfig.setProperty(QLatin1String("fileTags"), scriptEngine->toScriptValue(fileTags));
    if (!defaultModuleName.isEmpty())
        artifactConfig.setProperty(QLatin1String("module"), config.property("modules").property(defaultModuleName));
    return artifactConfig;
}

QScriptValue Transformer::translateInOutputs(QScriptEngine *scriptEngine, const QSet<Artifact*> &artifacts, const QString &defaultModuleName)
{
    typedef QMap<QString, QList<Artifact*> > TagArtifactsMap;
    TagArtifactsMap tagArtifactsMap;
    foreach (Artifact *artifact, artifacts)
        foreach (const QString &fileTag, artifact->fileTags)
            tagArtifactsMap[fileTag].append(artifact);

    QScriptValue jsTagFiles = scriptEngine->newObject();
    for (TagArtifactsMap::const_iterator tag = tagArtifactsMap.constBegin(); tag != tagArtifactsMap.constEnd(); ++tag) {
        const QList<Artifact*> &artifactList = tag.value();
        QScriptValue jsFileConfig = scriptEngine->newArray(artifactList.count());
        int i=0;
        foreach (Artifact *artifact, artifactList) {
            jsFileConfig.setProperty(i++, translateFileConfig(scriptEngine, artifact, defaultModuleName));
        }
        jsTagFiles.setProperty(tag.key(), jsFileConfig);
    }

    return jsTagFiles;
}

void Transformer::setupInputs(QScriptEngine *scriptEngine, QScriptValue targetScriptValue)
{
    const QString &defaultModuleName = rule->module->name;
    QScriptValue scriptValue = translateInOutputs(scriptEngine, inputs, defaultModuleName);
    targetScriptValue.setProperty("inputs", scriptValue);
    if (inputs.count() == 1) {
        Artifact *input = *inputs.begin();
        const QSet<QString> &fileTags = input->fileTags;
        QScriptValue inputsForFileTag = scriptValue.property(*fileTags.begin());
        QScriptValue inputScriptValue = inputsForFileTag.property(0);
        targetScriptValue.setProperty("input", inputScriptValue);
    }
}

void Transformer::setupOutputs(QScriptEngine *scriptEngine, QScriptValue targetScriptValue)
{
    const QString &defaultModuleName = rule->module->name;
    QScriptValue scriptValue = translateInOutputs(scriptEngine, outputs, defaultModuleName);
    targetScriptValue.setProperty("outputs", scriptValue);
    if (outputs.count() == 1) {
        Artifact *output = *outputs.begin();
        const QSet<QString> &fileTags = output->fileTags;
        Q_ASSERT(!fileTags.isEmpty());
        QScriptValue outputsForFileTag = scriptValue.property(*fileTags.begin());
        QScriptValue outputScriptValue = outputsForFileTag.property(0);
        targetScriptValue.setProperty("output", outputScriptValue);
    }
}

} // namespace qbs
