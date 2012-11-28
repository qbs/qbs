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

#include "forward_decls.h"
#include "jsimports.h"
#include <tools/codelocation.h>
#include <tools/fileinfo.h>
#include <tools/persistentobject.h>
#include <tools/settings.h>
#include <tools/weakpointer.h>

#include <QDataStream>
#include <QMutex>
#include <QProcessEnvironment>
#include <QScriptProgram>
#include <QScriptValue>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>

QT_BEGIN_NAMESPACE
class QScriptEngine;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {

class PropertyMap : public PersistentObject
{
public:
    typedef QSharedPointer<PropertyMap> Ptr;
    typedef QSharedPointer<const PropertyMap> ConstPtr;

    static Ptr create() { return Ptr(new PropertyMap); }
    Ptr clone() const { return Ptr(new PropertyMap(*this)); }

    const QVariantMap &value() const { return m_value; }
    void setValue(const QVariantMap &value);
    QScriptValue toScriptValue(QScriptEngine *scriptEngine) const;

private:
    PropertyMap();
    PropertyMap(const PropertyMap &other);

    void load(PersistentPool &);
    void store(PersistentPool &) const;

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

    void load(PersistentPool &);
    void store(PersistentPool &) const;

    QRegExp m_artifactExpression;
    QStringList m_fileTags;
};

class RuleArtifact : public PersistentObject
{
public:
    static RuleArtifactPtr create() { return RuleArtifactPtr(new RuleArtifact); }

    QString fileName;
    QStringList fileTags;
    bool alwaysUpdated;

    struct Binding
    {
        QStringList name;
        QString code;
        CodeLocation location;
    };

    QVector<Binding> bindings;

private:
    RuleArtifact()
        : alwaysUpdated(true)
    {}

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

class SourceArtifact : public PersistentObject
{
public:
    static SourceArtifactPtr create() { return SourceArtifactPtr(new SourceArtifact); }

    QString absoluteFilePath;
    QSet<QString> fileTags;
    bool overrideFileTags;
    PropertyMapPtr properties;

private:
    SourceArtifact() : overrideFileTags(true) {}

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

class SourceWildCards : public PersistentObject
{
public:
    typedef QSharedPointer<SourceWildCards> Ptr;
    typedef QSharedPointer<const SourceWildCards> ConstPtr;

    static Ptr create() { return Ptr(new SourceWildCards); }

    QSet<QString> expandPatterns(const QString &baseDir) const;

    // TODO: Use back pointer to Group instead?
    bool recursive;
    QString prefix;

    QStringList patterns;
    QStringList excludePatterns;
    QList<SourceArtifactPtr> files;

private:
    SourceWildCards() : recursive(false) {}

    QSet<QString> expandPatterns(const QStringList &patterns, const QString &baseDir) const;
    void expandPatterns(QSet<QString> &files, const QString &baseDir, bool useRecursion,
                      const QStringList &parts, int index = 0) const;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

class ResolvedGroup : public PersistentObject
{
public:
    typedef QSharedPointer<ResolvedGroup> Ptr;
    typedef QSharedPointer<const ResolvedGroup> ConstPtr;

    static Ptr create() { return Ptr(new ResolvedGroup); }

    int qbsLine;

    QString name;
    QList<SourceArtifactPtr> files;
    SourceWildCards::Ptr wildcards;
    PropertyMapPtr properties;

    QList<SourceArtifactPtr> allFiles() const;

private:
    ResolvedGroup() {}

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

class PrepareScript: public PersistentObject
{
public:
    static PrepareScriptPtr create() { return PrepareScriptPtr(new PrepareScript); }

    QString script;
    CodeLocation location;

private:
    PrepareScript() {}

    void load(PersistentPool &);
    void store(PersistentPool &) const;
};

class ResolvedModule : public PersistentObject
{
public:
    static ResolvedModulePtr create() { return ResolvedModulePtr(new ResolvedModule); }

    QString name;
    QStringList moduleDependencies;
    JsImports jsImports;
    QString setupBuildEnvironmentScript;
    QString setupRunEnvironmentScript;

private:
    ResolvedModule() {}

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
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
    static RulePtr create() { return RulePtr(new Rule); }

    ResolvedModuleConstPtr module;
    JsImports jsImports;
    PrepareScriptConstPtr script;
    QStringList inputs;
    QStringList usings;
    QStringList explicitlyDependsOn;
    bool multiplex;
    QList<RuleArtifactConstPtr> artifacts;

    // members that we don't need to save
    int ruleGraphId;

    QString toString() const;
    QStringList outputFileTags() const;

private:
    Rule() : multiplex(false), ruleGraphId(-1) {}

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

class ResolvedTransformer
{
public:
    typedef QSharedPointer<ResolvedTransformer> Ptr;

    static Ptr create() { return Ptr(new ResolvedTransformer); }

    ResolvedModuleConstPtr module;
    QStringList inputs;
    QList<SourceArtifactPtr> outputs;
    PrepareScriptConstPtr transform;
    JsImports jsImports;

private:
    ResolvedTransformer() {}
};

class ResolvedProject;
class ScriptEngine;

class ResolvedProduct : public PersistentObject
{
public:
    static ResolvedProductPtr create() { return ResolvedProductPtr(new ResolvedProduct); }

    QStringList fileTags;
    QStringList additionalFileTags;
    QString name;
    QString targetName;
    QString sourceDirectory;
    QString destinationDirectory;
    QString qbsFile;
    int qbsLine;
    WeakPointer<ResolvedProject> project;
    PropertyMapPtr properties;
    QSet<RulePtr> rules;
    QSet<ResolvedProductPtr> dependencies;
    QSet<FileTagger::ConstPtr> fileTaggers;
    QList<ResolvedModuleConstPtr> modules;
    QList<ResolvedTransformer::Ptr> transformers;
    QList<ResolvedGroup::Ptr> groups;

    mutable QProcessEnvironment buildEnvironment; // must not be saved
    mutable QProcessEnvironment runEnvironment; // must not be saved
    QHash<QString, QString> executablePathCache;

    QList<SourceArtifactPtr> allFiles() const;
    QSet<QString> fileTagsForFileName(const QString &fileName) const;
    void setupBuildEnvironment(ScriptEngine *scriptEngine, const QProcessEnvironment &systemEnvironment) const;
    void setupRunEnvironment(ScriptEngine *scriptEngine, const QProcessEnvironment &systemEnvironment) const;

private:
    ResolvedProduct();

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

class ResolvedProject: public PersistentObject
{
public:
    static ResolvedProjectPtr create() { return ResolvedProjectPtr(new ResolvedProject); }

    static QString deriveId(const QVariantMap &config);
    static QString deriveBuildDirectory(const QString &buildRoot, const QString &id);

    QString qbsFile; // Not saved.
    QString buildDirectory; // Not saved
    QVariantMap platformEnvironment;
    QList<ResolvedProductPtr> products;

    void setBuildConfiguration(const QVariantMap &config);
    const QVariantMap &buildConfiguration() const { return m_buildConfiguration; }
    QString id() const { return m_id; }

private:
    ResolvedProject() {}

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

    QVariantMap m_buildConfiguration;
    QString m_id;
};

} // namespace Internal
} // namespace qbs

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(qbs::Internal::JsImport, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(qbs::Internal::RuleArtifact::Binding, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

#endif
