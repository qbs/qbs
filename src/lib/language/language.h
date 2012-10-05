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

#include <QDataStream>
#include <QMutex>
#include <QProcessEnvironment>
#include <QScriptProgram>
#include <QScriptValue>
#include <QSet>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QVariant>

QT_BEGIN_NAMESPACE
class QScriptEngine;
QT_END_NAMESPACE

namespace qbs {

/**
  * Represents JavaScript import of the form
  *    import 'fileOrDirectory' as scopeName
  *
  * There can be several filenames per scope
  * if we import a whole directory.
  */
class JsImport
{
public:
    QString scopeName;
    QStringList fileNames;
    CodeLocation location;
};

typedef QList<JsImport> JsImports;

class Rule;

class Configuration: public qbs::PersistentObject
{
public:
    typedef QSharedPointer<Configuration> Ptr;
    typedef QSharedPointer<const Configuration> ConstPtr;

    Configuration();
    Configuration(const Configuration &other);

    const QVariantMap &value() const { return m_value; }
    void setValue(const QVariantMap &value);
    QScriptValue cachedScriptValue(QScriptEngine *scriptEngine) const;
    void cacheScriptValue(const QScriptValue &scriptValue);

private:
    void load(qbs::PersistentPool &, QDataStream &s);
    void store(qbs::PersistentPool &, QDataStream &s) const;

    QVariantMap m_value;
    QHash<QScriptEngine *, QScriptValue> m_scriptValueCache;
    mutable QMutex m_scriptValueCacheMutex;
};

class FileTagger : public qbs::PersistentObject
{
public:
    typedef QSharedPointer<FileTagger> Ptr;
    typedef QSharedPointer<const FileTagger> ConstPtr;

    FileTagger(const QRegExp &artifactExpression, const QStringList &fileTags)
        : m_artifactExpression(artifactExpression), m_fileTags(fileTags)
    { }

    FileTagger()
        : m_artifactExpression(QString(), Qt::CaseSensitive, QRegExp::Wildcard)
    {}

    const QRegExp &artifactExpression() const { return m_artifactExpression; }
    const QStringList &fileTags() const { return m_fileTags; }

private:
    QRegExp m_artifactExpression;
    QStringList m_fileTags;

    void load(qbs::PersistentPool &, QDataStream &s);
    void store(qbs::PersistentPool &, QDataStream &s) const;
};

class RuleArtifact : public qbs::PersistentObject
{
public:
    typedef QSharedPointer<RuleArtifact> Ptr;
    typedef QSharedPointer<const RuleArtifact> ConstPtr;

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
    void load(qbs::PersistentPool &pool, QDataStream &s);
    void store(qbs::PersistentPool &pool, QDataStream &s) const;
};

class SourceArtifact : public qbs::PersistentObject
{
public:
    typedef QSharedPointer<SourceArtifact> Ptr;
    typedef QSharedPointer<const SourceArtifact> ConstPtr;

    QString absoluteFilePath;
    QSet<QString> fileTags;
    bool overrideFileTags;
    Configuration::Ptr configuration;

    SourceArtifact()
        : overrideFileTags(true)
    {}

private:
    void load(qbs::PersistentPool &pool, QDataStream &s);
    void store(qbs::PersistentPool &pool, QDataStream &s) const;
};

class RuleScript: public qbs::PersistentObject
{
public:
    typedef QSharedPointer<RuleScript> Ptr;
    typedef QSharedPointer<const RuleScript> ConstPtr;

    QString script;
    qbs::CodeLocation location;

private:
    void load(qbs::PersistentPool &, QDataStream &s);
    void store(qbs::PersistentPool &, QDataStream &s) const;
};

class ResolvedModule : public qbs::PersistentObject
{
public:
    typedef QSharedPointer<ResolvedModule> Ptr;
    typedef QSharedPointer<const ResolvedModule> ConstPtr;

    QString name;
    QStringList moduleDependencies;
    JsImports jsImports;
    QString setupBuildEnvironmentScript;
    QString setupRunEnvironmentScript;

private:
    void load(qbs::PersistentPool &pool, QDataStream &s);
    void store(qbs::PersistentPool &pool, QDataStream &s) const;
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
    typedef QSharedPointer<const Rule> ConstPtr;

    ResolvedModule::ConstPtr module;
    JsImports jsImports;
    RuleScript::ConstPtr script;
    QStringList inputs;
    QStringList usings;
    QStringList explicitlyDependsOn;
    bool multiplex;
    QString objectId;
    QList<RuleArtifact::ConstPtr> artifacts;
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
    void load(qbs::PersistentPool &pool, QDataStream &s);
    void store(qbs::PersistentPool &pool, QDataStream &s) const;
};

class Group : public qbs::PersistentObject
{
public:
    typedef QSharedPointer<Group> Ptr;
    typedef QSharedPointer<const Group> ConstPtr;

    QString prefix;
    bool recursive;
    QStringList patterns;
    QStringList excludePatterns;
    QSet<QString> files;

    Group() : recursive(false) {}

private:
    void load(qbs::PersistentPool &pool, QDataStream &s);
    void store(qbs::PersistentPool &pool, QDataStream &s) const;
};

class ResolvedTransformer
{
public:
    typedef QSharedPointer<ResolvedTransformer> Ptr;

    ResolvedModule::ConstPtr module;
    QStringList inputs;
    QList<SourceArtifact::Ptr> outputs;
    RuleScript::ConstPtr transform;
    JsImports jsImports;
};

class ResolvedProject;
class ResolvedProduct: public qbs::PersistentObject
{
public:
    typedef QSharedPointer<ResolvedProduct> Ptr;
    typedef QSharedPointer<const ResolvedProduct> ConstPtr;

    ResolvedProduct();

    QStringList fileTags;
    QString name;
    QString targetName;
    QString buildDirectory;
    QString sourceDirectory;
    QString destinationDirectory;
    QString qbsFile;
    ResolvedProject *project;
    Configuration::Ptr configuration;
    QSet<SourceArtifact::Ptr> sources;
    QSet<Rule::Ptr> rules;
    QSet<ResolvedProduct::Ptr> uses;
    QSet<FileTagger::ConstPtr> fileTaggers;
    QList<ResolvedModule::ConstPtr> modules;
    QList<ResolvedTransformer::Ptr> transformers;
    QList<Group::ConstPtr> groups;

    mutable QProcessEnvironment buildEnvironment; // must not be saved
    mutable QProcessEnvironment runEnvironment; // must not be saved
    QHash<QString, QString> executablePathCache;

    QSet<QString> fileTagsForFileName(const QString &fileName) const;
    void setupBuildEnvironment(QScriptEngine *scriptEngine, const QProcessEnvironment &systemEnvironment) const;
    void setupRunEnvironment(QScriptEngine *scriptEngine, const QProcessEnvironment &systemEnvironment) const;

private:
    void load(qbs::PersistentPool &pool, QDataStream &s);
    void store(qbs::PersistentPool &pool, QDataStream &s) const;
};

class ResolvedProject: public qbs::PersistentObject
{
public:
    typedef QSharedPointer<ResolvedProject> Ptr;
    QString id;
    QString qbsFile;
    QVariantMap platformEnvironment;
    QSet<ResolvedProduct::Ptr> products;
    Configuration::Ptr configuration;

private:
    void load(qbs::PersistentPool &pool, QDataStream &s);
    void store(qbs::PersistentPool &pool, QDataStream &s) const;
};

} // namespace qbs

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(qbs::JsImport, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(qbs::RuleArtifact::Binding, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

#endif
