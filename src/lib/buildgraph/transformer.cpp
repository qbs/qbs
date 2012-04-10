/*************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

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
    QScriptValue config = artifact->configuration->cachedScriptValue(scriptEngine);
    if (!config.isValid()) {
        config = scriptEngine->toScriptValue(artifact->configuration->value());
        artifact->configuration->cacheScriptValue(scriptEngine, config);
    }

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
    targetScriptValue.setProperty("inputs", scriptValue, QScriptValue::ReadOnly);
    if (inputs.count() == 1) {
        Artifact *input = *inputs.begin();
        const QSet<QString> &fileTags = input->fileTags;
        QScriptValue inputsForFileTag = scriptValue.property(*fileTags.begin());
        QScriptValue inputScriptValue = inputsForFileTag.property(0);
        targetScriptValue.setProperty("input", inputScriptValue, QScriptValue::ReadOnly);
    }
}

void Transformer::setupOutputs(QScriptEngine *scriptEngine, QScriptValue targetScriptValue)
{
    const QString &defaultModuleName = rule->module->name;
    QScriptValue scriptValue = translateInOutputs(scriptEngine, outputs, defaultModuleName);
    targetScriptValue.setProperty("outputs", scriptValue, QScriptValue::ReadOnly);
    if (outputs.count() == 1) {
        Artifact *output = *outputs.begin();
        const QSet<QString> &fileTags = output->fileTags;
        Q_ASSERT(!fileTags.isEmpty());
        QScriptValue outputsForFileTag = scriptValue.property(*fileTags.begin());
        QScriptValue outputScriptValue = outputsForFileTag.property(0);
        targetScriptValue.setProperty("output", outputScriptValue, QScriptValue::ReadOnly);
    }
}

} // namespace qbs
