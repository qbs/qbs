/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
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

#ifndef L2_LANGUAGE_HPP
#define L2_LANGUAGE_HPP

#include <tools/codelocation.h>
#include <tools/persistence.h>
#include <tools/settings.h>
#include <tools/fileinfo.h>

#include <QtCore/QString>
#include <QtCore/QDataStream>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QStringList>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariant>
#include <QtCore/QMutex>
#include <QtScript/QScriptProgram>
#include <QtScript/QScriptValue>

QT_BEGIN_NAMESPACE
class QScriptEngine;
QT_END_NAMESPACE

namespace qbs {

/**
  * Represents JavaScript import.
  * Key: scope name, Value: list of JS file names.
  * There can be several filenames per scope
  * if we import a whole directory.
  */
typedef QHash<QString, QStringList> JsImports;

class Rule;

class Configuration: public qbs::PersistentObject
{
public:
    typedef QSharedPointer<Configuration> Ptr;

    const QVariantMap &value() const { return m_value; }
    void setValue(const QVariantMap &value);
    QScriptValue cachedScriptValue(QScriptEngine *scriptEngine) const;
    void cacheScriptValue(QScriptEngine *scriptEngine, const QScriptValue &scriptValue);

private:
    void load(qbs::PersistentPool &, qbs::PersistentObjectData &data);
    void store(qbs::PersistentPool &, qbs::PersistentObjectData &data) const;

private:
    QVariantMap m_value;
    QHash<QScriptEngine *, QScriptValue> m_scriptValueCache;
    static QMutex m_scriptValueCacheMutex;
};

class FileTagger : public qbs::PersistentObject
{
public:
    typedef QSharedPointer<FileTagger> Ptr;
    QRegExp artifactExpression;
    QStringList fileTags;

    FileTagger()
        : artifactExpression(QString(), Qt::CaseSensitive, QRegExp::Wildcard)
    {}

private:
    void load(qbs::PersistentPool &, qbs::PersistentObjectData &data);
    void store(qbs::PersistentPool &, qbs::PersistentObjectData &data) const;
};

class RuleArtifact : public qbs::PersistentObject
{
public:
    typedef QSharedPointer<RuleArtifact> Ptr;
    QString fileScript;
    QStringList fileTags;

    struct Binding
    {
        QStringList name;
        QString code;
        CodeLocation location;
    };

    QVector<Binding> bindings;

private:
    void load(qbs::PersistentPool &pool, qbs::PersistentObjectData &data);
    void store(qbs::PersistentPool &pool, qbs::PersistentObjectData &data) const;
};

class SourceArtifact : public qbs::PersistentObject
{
public:
    typedef QSharedPointer<SourceArtifact> Ptr;
    QString absoluteFilePath;
    QSet<QString> fileTags;
    Configuration::Ptr configuration;

private:
    void load(qbs::PersistentPool &pool, qbs::PersistentObjectData &data);
    void store(qbs::PersistentPool &pool, qbs::PersistentObjectData &data) const;
};

class RuleScript: public qbs::PersistentObject
{
public:
    typedef QSharedPointer<RuleScript> Ptr;
    QString script;
    qbs::CodeLocation location;

private:
    void load(qbs::PersistentPool &, qbs::PersistentObjectData &data);
    void store(qbs::PersistentPool &, qbs::PersistentObjectData &data) const;
};

class ResolvedModule : public qbs::PersistentObject
{
public:
    typedef QSharedPointer<ResolvedModule> Ptr;
    QString name;
    QStringList moduleDependencies;
    JsImports jsImports;
    QString setupBuildEnvironmentScript;
    QString setupRunEnvironmentScript;

private:
    void load(qbs::PersistentPool &pool, qbs::PersistentObjectData &data);
    void store(qbs::PersistentPool &pool, qbs::PersistentObjectData &data) const;
};

/**
  * Per default each rule is a "non-multiplex rule".
  *
  * A "multiplex rule" creates one transformer that takes all
  * input artifacts with the matching input file tag and creates
  * one or more artifacts. (e.g. linker rule)
  *
  * A "non-multiplex rule" creates one transformer per matching input file.
  */
class Rule : public qbs::PersistentObject
{
public:
    typedef QSharedPointer<Rule> Ptr;
    ResolvedModule::Ptr module;
    JsImports jsImports;
    RuleScript::Ptr script;
    QStringList inputs;
    QStringList usings;
    QStringList explicitlyDependsOn;
    bool multiplex;
    QString objectId;
    QList<RuleArtifact::Ptr> artifacts;
    QMap<QString, QScriptProgram> transformProperties;

    // members that we don't need to save
    int ruleGraphId;

    Rule()
        : multiplex(false)
        , ruleGraphId(-1)
    {}

    QString toString() const;
    QStringList outputFileTags() const;

    inline bool isMultiplexRule() const
    {
        return multiplex;
    }

private:
    void load(qbs::PersistentPool &pool, qbs::PersistentObjectData &data);
    void store(qbs::PersistentPool &pool, qbs::PersistentObjectData &data) const;
};

class ResolvedTransformer
{
public:
    typedef QSharedPointer<ResolvedTransformer> Ptr;

    ResolvedModule::Ptr module;
    QStringList inputs;
    QList<SourceArtifact::Ptr> outputs;
    RuleScript::Ptr transform;
    JsImports jsImports;
};

class ResolvedProject;
class ResolvedProduct: public qbs::PersistentObject
{
public:
    typedef QSharedPointer<ResolvedProduct> Ptr;
    ResolvedProduct();

    QStringList fileTags;
    QString name;
    QString buildDirectory;
    QString sourceDirectory;
    QString destinationDirectory;
    QString qbsFile;
    ResolvedProject *project;
    Configuration::Ptr configuration;
    QSet<SourceArtifact::Ptr> sources;
    QSet<Rule::Ptr> rules;
    QSet<ResolvedProduct::Ptr> uses;
    QSet<FileTagger::Ptr> fileTaggers;
    QList<ResolvedModule::Ptr> modules;
    QList<ResolvedTransformer::Ptr> transformers;

    mutable QProcessEnvironment buildEnvironment; // must not be saved
    mutable QProcessEnvironment runEnvironment; // must not be saved
    QHash<QString, QString> executablePathCache;

    QSet<QString> fileTagsForFileName(const QString &fileName) const;
    void setupBuildEnvironment(QScriptEngine *scriptEngine, const QProcessEnvironment &systemEnvironment) const;
    void setupRunEnvironment(QScriptEngine *scriptEngine, const QProcessEnvironment &systemEnvironment) const;

private:
    void load(qbs::PersistentPool &pool, qbs::PersistentObjectData &data);
    void store(qbs::PersistentPool &pool, qbs::PersistentObjectData &data) const;
};

class ResolvedProject: public qbs::PersistentObject
{
public:
    typedef QSharedPointer<ResolvedProject> Ptr;
    QString id;
    QString qbsFile;
    QSet<ResolvedProduct::Ptr> products;
    Configuration::Ptr configuration;

private:
    void load(qbs::PersistentPool &pool, qbs::PersistentObjectData &data);
    void store(qbs::PersistentPool &pool, qbs::PersistentObjectData &data) const;
};

} // namespace qbs

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(qbs::RuleArtifact::Binding, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

#endif
