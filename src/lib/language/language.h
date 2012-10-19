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

#ifndef L2_LANGUAGE_HPP
#define L2_LANGUAGE_HPP

#include "jsimports.h"
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

class Rule;

class Configuration: public PersistentObject
{
public:
    typedef QSharedPointer<Configuration> Ptr;
    typedef QSharedPointer<const Configuration> ConstPtr;

    static Ptr create() { return Ptr(new Configuration); }
    static Ptr create(const Configuration &other) { return Ptr(new Configuration(other)); }

    const QVariantMap &value() const { return m_value; }
    void setValue(const QVariantMap &value);
    QScriptValue toScriptValue(QScriptEngine *scriptEngine) const;

private:
    Configuration();
    Configuration(const Configuration &other);

    void load(PersistentPool &, QDataStream &s);
    void store(PersistentPool &, QDataStream &s) const;

    QVariantMap m_value;
    mutable QHash<QScriptEngine *, QScriptValue> m_scriptValueCache;
    mutable QMutex m_scriptValueCacheMutex;
};

class FileTagger : public PersistentObject
{
public:
    typedef QSharedPointer<FileTagger> Ptr;
    typedef QSharedPointer<const FileTagger> ConstPtr;

    static Ptr create() { return Ptr(new FileTagger); }
    static Ptr create(const QRegExp &artifactExpression, const QStringList &fileTags) {
        return Ptr(new FileTagger(artifactExpression, fileTags));
    }

    const QRegExp &artifactExpression() const { return m_artifactExpression; }
    const QStringList &fileTags() const { return m_fileTags; }

private:
    FileTagger(const QRegExp &artifactExpression, const QStringList &fileTags)
        : m_artifactExpression(artifactExpression), m_fileTags(fileTags)
    { }

    FileTagger()
        : m_artifactExpression(QString(), Qt::CaseSensitive, QRegExp::Wildcard)
    {}

    void load(PersistentPool &, QDataStream &s);
    void store(PersistentPool &, QDataStream &s) const;

    QRegExp m_artifactExpression;
    QStringList m_fileTags;
};

class RuleArtifact : public PersistentObject
{
public:
    typedef QSharedPointer<RuleArtifact> Ptr;
    typedef QSharedPointer<const RuleArtifact> ConstPtr;

    static Ptr create() { return Ptr(new RuleArtifact); }

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
    RuleArtifact() {}

    void load(PersistentPool &pool, QDataStream &s);
    void store(PersistentPool &pool, QDataStream &s) const;
};

class Group;

class SourceArtifact : public PersistentObject
{
public:
    typedef QSharedPointer<SourceArtifact> Ptr;
    typedef QSharedPointer<const SourceArtifact> ConstPtr;

    static Ptr create() { return Ptr(new SourceArtifact); }

    QString absoluteFilePath;
    QSet<QString> fileTags;
    bool overrideFileTags;
    Configuration::Ptr configuration;

private:
    SourceArtifact() : overrideFileTags(true) {}

    void load(PersistentPool &pool, QDataStream &s);
    void store(PersistentPool &pool, QDataStream &s) const;
};

class SourceWildCards : public PersistentObject
{
public:
    typedef QSharedPointer<SourceWildCards> Ptr;
    typedef QSharedPointer<const SourceWildCards> ConstPtr;

    static Ptr create() { return Ptr(new SourceWildCards); }

    bool recursive;
    QString prefix;
    QStringList patterns;
    QStringList excludePatterns;
    QList<SourceArtifact::Ptr> files;

private:
    SourceWildCards() : recursive(false) {}

    void load(PersistentPool &pool, QDataStream &s);
    void store(PersistentPool &pool, QDataStream &s) const;
};

class Group : public PersistentObject
{
public:
    typedef QSharedPointer<Group> Ptr;
    typedef QSharedPointer<const Group> ConstPtr;

    static Ptr create() { return Ptr(new Group); }

    QString name;
    QList<SourceArtifact::Ptr> files;
    SourceWildCards::Ptr wildcards;     // can be null
    Configuration::Ptr configuration;

    QList<SourceArtifact::Ptr> allFiles() const;

private:
    Group() {}

    void load(PersistentPool &pool, QDataStream &s);
    void store(PersistentPool &pool, QDataStream &s) const;
};

class RuleScript: public PersistentObject
{
public:
    typedef QSharedPointer<RuleScript> Ptr;
    typedef QSharedPointer<const RuleScript> ConstPtr;

    static Ptr create() { return Ptr(new RuleScript); }

    QString script;
    CodeLocation location;

private:
    RuleScript() {}

    void load(PersistentPool &, QDataStream &s);
    void store(PersistentPool &, QDataStream &s) const;
};

class ResolvedModule : public PersistentObject
{
public:
    typedef QSharedPointer<ResolvedModule> Ptr;
    typedef QSharedPointer<const ResolvedModule> ConstPtr;

    static Ptr create() { return Ptr(new ResolvedModule); }

    QString name;
    QStringList moduleDependencies;
    JsImports jsImports;
    QString setupBuildEnvironmentScript;
    QString setupRunEnvironmentScript;

private:
    ResolvedModule() {}

    void load(PersistentPool &pool, QDataStream &s);
    void store(PersistentPool &pool, QDataStream &s) const;
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
class Rule : public PersistentObject
{
public:
    typedef QSharedPointer<Rule> Ptr;
    typedef QSharedPointer<const Rule> ConstPtr;

    static Ptr create() { return Ptr(new Rule); }

    ResolvedModule::ConstPtr module;
    JsImports jsImports;
    RuleScript::ConstPtr script;
    QStringList inputs;
    QStringList usings;
    QStringList explicitlyDependsOn;
    bool multiplex;
    QString objectId;
    QList<RuleArtifact::ConstPtr> artifacts;

    // members that we don't need to save
    int ruleGraphId;

    QString toString() const;
    QStringList outputFileTags() const;

    inline bool isMultiplexRule() const
    {
        return multiplex;
    }

private:
    Rule() : multiplex(false), ruleGraphId(-1) {}

    void load(PersistentPool &pool, QDataStream &s);
    void store(PersistentPool &pool, QDataStream &s) const;
};

class ResolvedTransformer
{
public:
    typedef QSharedPointer<ResolvedTransformer> Ptr;

    static Ptr create() { return Ptr(new ResolvedTransformer); }

    ResolvedModule::ConstPtr module;
    QStringList inputs;
    QList<SourceArtifact::Ptr> outputs;
    RuleScript::ConstPtr transform;
    JsImports jsImports;

private:
    ResolvedTransformer() {}
};

class ResolvedProject;
class QbsEngine;

class ResolvedProduct: public PersistentObject
{
public:
    typedef QSharedPointer<ResolvedProduct> Ptr;
    typedef QSharedPointer<const ResolvedProduct> ConstPtr;

    static Ptr create() { return Ptr(new ResolvedProduct); }

    QStringList fileTags;
    QString name;
    QString targetName;
    QString buildDirectory;
    QString sourceDirectory;
    QString destinationDirectory;
    QString qbsFile;
    ResolvedProject *project;
    Configuration::Ptr configuration;
    QSet<Rule::Ptr> rules;
    QSet<ResolvedProduct::Ptr> uses;
    QSet<FileTagger::ConstPtr> fileTaggers;
    QList<ResolvedModule::ConstPtr> modules;
    QList<ResolvedTransformer::Ptr> transformers;
    QList<Group::Ptr> groups;

    mutable QProcessEnvironment buildEnvironment; // must not be saved
    mutable QProcessEnvironment runEnvironment; // must not be saved
    QHash<QString, QString> executablePathCache;

    QList<SourceArtifact::Ptr> allFiles() const;
    QSet<QString> fileTagsForFileName(const QString &fileName) const;
    void setupBuildEnvironment(QbsEngine *scriptEngine, const QProcessEnvironment &systemEnvironment) const;
    void setupRunEnvironment(QbsEngine *scriptEngine, const QProcessEnvironment &systemEnvironment) const;

private:
    ResolvedProduct();

    void load(PersistentPool &pool, QDataStream &s);
    void store(PersistentPool &pool, QDataStream &s) const;
};

class ResolvedProject: public PersistentObject
{
public:
    typedef QSharedPointer<ResolvedProject> Ptr;

    static Ptr create() { return Ptr(new ResolvedProject); }

    static QString deriveId(const QVariantMap &config);

    QString qbsFile;
    QVariantMap platformEnvironment;
    QSet<ResolvedProduct::Ptr> products;

    void setConfiguration(const QVariantMap &config);
    QVariantMap configuration() const { return m_configuration->value(); }
    QString id() const { return m_id; }

private:
    ResolvedProject() {}

    void load(PersistentPool &pool, QDataStream &s);
    void store(PersistentPool &pool, QDataStream &s) const;

    Configuration::Ptr m_configuration;
    QString m_id;
};

} // namespace qbs

Q_DECLARE_METATYPE(qbs::ResolvedProject::Ptr);

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(qbs::JsImport, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(qbs::RuleArtifact::Binding, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

#endif
