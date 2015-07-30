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

#ifndef QBS_LANGUAGE_H
#define QBS_LANGUAGE_H

#include "filetags.h"
#include "forward_decls.h"
#include "jsimports.h"

#include <buildgraph/forward_decls.h>
#include <tools/codelocation.h>
#include <tools/filetime.h>
#include <tools/persistentobject.h>
#include <tools/weakpointer.h>

#include <QDataStream>
#include <QHash>
#include <QMutex>
#include <QProcessEnvironment>
#include <QRegExp>
#include <QScriptValue>
#include <QScopedPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>

QT_BEGIN_NAMESPACE
class QScriptEngine;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {
class BuildGraphLocker;
class BuildGraphLoader;
class BuildGraphVisitor;

class FileTagger : public PersistentObject
{
public:
    static FileTaggerPtr create() { return FileTaggerPtr(new FileTagger); }
    static FileTaggerPtr create(const QStringList &patterns, const FileTags &fileTags) {
        return FileTaggerPtr(new FileTagger(patterns, fileTags));
    }

    const QList<QRegExp> &patterns() const { return m_patterns; }
    const FileTags &fileTags() const { return m_fileTags; }

private:
    FileTagger(const QStringList &patterns, const FileTags &fileTags);
    FileTagger() {}

    void setPatterns(const QStringList &patterns);

    void load(PersistentPool &);
    void store(PersistentPool &) const;

    QList<QRegExp> m_patterns;
    FileTags m_fileTags;
};

class RuleArtifact : public PersistentObject
{
public:
    static RuleArtifactPtr create() { return RuleArtifactPtr(new RuleArtifact); }

    QString filePath;
    FileTags fileTags;
    bool alwaysUpdated;
    CodeLocation location;

    class Binding
    {
    public:
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
uint qHash(const RuleArtifact::Binding &b);
bool operator==(const RuleArtifact::Binding &b1, const RuleArtifact::Binding &b2);
inline bool operator!=(const RuleArtifact::Binding &b1, const RuleArtifact::Binding &b2) {
    return !(b1 == b2);
}
bool operator==(const RuleArtifact &a1, const RuleArtifact &a2);
inline bool operator!=(const RuleArtifact &a1, const RuleArtifact &a2) { return !(a1 == a2); }

class SourceArtifactInternal : public PersistentObject
{
public:
    static SourceArtifactPtr create() { return SourceArtifactPtr(new SourceArtifactInternal); }

    QString absoluteFilePath;
    FileTags fileTags;
    bool overrideFileTags;
    PropertyMapPtr properties;

private:
    SourceArtifactInternal() : overrideFileTags(true) {}

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};
bool operator==(const SourceArtifactInternal &sa1, const SourceArtifactInternal &sa2);
inline bool operator!=(const SourceArtifactInternal &sa1, const SourceArtifactInternal &sa2) {
    return !(sa1 == sa2);
}

bool sourceArtifactSetsAreEqual(const QList<SourceArtifactPtr> &l1,
                                 const QList<SourceArtifactPtr> &l2);

class SourceWildCards : public PersistentObject
{
public:
    typedef QSharedPointer<SourceWildCards> Ptr;
    typedef QSharedPointer<const SourceWildCards> ConstPtr;

    static Ptr create() { return Ptr(new SourceWildCards); }

    QSet<QString> expandPatterns(const GroupConstPtr &group, const QString &baseDir) const;

    // TODO: Use back pointer to Group instead?
    QString prefix;

    QStringList patterns;
    QStringList excludePatterns;
    QList<SourceArtifactPtr> files;

private:
    SourceWildCards() {}

    QSet<QString> expandPatterns(const GroupConstPtr &group, const QStringList &patterns,
                                 const QString &baseDir) const;
    void expandPatterns(QSet<QString> &result, const GroupConstPtr &group,
                        const QStringList &parts, const QString &baseDir) const;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

class ResolvedGroup : public PersistentObject
{
public:
    static GroupPtr create() { return GroupPtr(new ResolvedGroup); }

    CodeLocation location;

    QString name;
    bool enabled;
    QString prefix;
    QList<SourceArtifactPtr> files;
    SourceWildCards::Ptr wildcards;
    PropertyMapPtr properties;
    FileTags fileTags;
    bool overrideTags;

    QList<SourceArtifactPtr> allFiles() const;

private:
    ResolvedGroup()
        : enabled(true)
    {}

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

class ScriptFunction : public PersistentObject
{
public:
    static ScriptFunctionPtr create() { return ScriptFunctionPtr(new ScriptFunction); }

    ~ScriptFunction();

    QString sourceCode;
    QStringList argumentNames;
    CodeLocation location;
    ResolvedFileContextConstPtr fileContext;
    mutable QScriptValue scriptFunction;    // cache

    bool isValid() const;

private:
    ScriptFunction();

    void load(PersistentPool &);
    void store(PersistentPool &) const;
};

bool operator==(const ScriptFunction &a, const ScriptFunction &b);
inline bool operator!=(const ScriptFunction &a, const ScriptFunction &b) { return !(a == b); }

class ResolvedModule : public PersistentObject
{
public:
    static ResolvedModulePtr create() { return ResolvedModulePtr(new ResolvedModule); }

    QString name;
    QStringList moduleDependencies;
    ScriptFunctionPtr setupBuildEnvironmentScript;
    ScriptFunctionPtr setupRunEnvironmentScript;

private:
    ResolvedModule() {}

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};
bool operator==(const ResolvedModule &m1, const ResolvedModule &m2);
inline bool operator!=(const ResolvedModule &m1, const ResolvedModule &m2) { return !(m1 == m2); }

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
    QString name;
    ScriptFunctionPtr prepareScript;
    FileTags outputFileTags;                    // unused, if artifacts is non-empty
    ScriptFunctionPtr outputArtifactsScript;    // unused, if artifacts is non-empty
    FileTags inputs;
    FileTags auxiliaryInputs;
    FileTags excludedAuxiliaryInputs;
    FileTags inputsFromDependencies;
    FileTags explicitlyDependsOn;
    bool multiplex;
    QList<RuleArtifactPtr> artifacts;           // unused, if outputFileTags/outputArtifactsScript is non-empty

    // members that we don't need to save
    int ruleGraphId;

    QString toString() const;
    bool acceptsAsInput(Artifact *artifact) const;
    FileTags staticOutputFileTags() const;
    FileTags collectedOutputFileTags() const;
    bool isDynamic() const;
private:
    Rule() : multiplex(false), ruleGraphId(-1) {}

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};
bool operator==(const Rule &r1, const Rule &r2);
inline bool operator!=(const Rule &r1, const Rule &r2) { return !(r1 == r2); }
bool ruleListsAreEqual(const QList<RulePtr> &l1, const QList<RulePtr> &l2);

class ResolvedTransformer : public PersistentObject
{
public:
    static ResolvedTransformerPtr create()
    {
        return ResolvedTransformerPtr(new ResolvedTransformer);
    }

    ResolvedModuleConstPtr module;
    QStringList inputs;
    QList<SourceArtifactPtr> outputs;
    ScriptFunctionPtr transform;
    FileTags explicitlyDependsOn;

private:
    ResolvedTransformer() {}

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

bool operator==(const ResolvedTransformer &t1, const ResolvedTransformer &t2);
inline bool operator!=(const ResolvedTransformer &t1, const ResolvedTransformer &t2) {
    return !(t1 == t2);
}
bool transformerListsAreEqual(const QList<ResolvedTransformerPtr> &l1,
                              const QList<ResolvedTransformerPtr> &l2);

class ResolvedScanner : public PersistentObject
{
public:
    static ResolvedScannerPtr create() { return ResolvedScannerPtr(new ResolvedScanner); }

    ResolvedModuleConstPtr module;
    FileTags inputs;
    bool recursive;
    ScriptFunctionPtr searchPathsScript;
    ScriptFunctionPtr scanScript;

private:
    ResolvedScanner() :
        recursive(false)
    {}

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

class TopLevelProject;
class ScriptEngine;

class ResolvedProduct : public PersistentObject
{
public:
    static ResolvedProductPtr create() { return ResolvedProductPtr(new ResolvedProduct); }

    ~ResolvedProduct();

    bool enabled;
    FileTags fileTags;
    QString name;
    QString targetName;
    QString profile;
    QString sourceDirectory;
    QString destinationDirectory;
    CodeLocation location;
    WeakPointer<ResolvedProject> project;
    QVariantMap productProperties;
    PropertyMapPtr moduleProperties;
    QSet<RulePtr> rules;
    QSet<ResolvedProductPtr> dependencies;
    QList<FileTaggerConstPtr> fileTaggers;
    QList<ResolvedModuleConstPtr> modules;
    QList<ResolvedTransformerPtr> transformers;
    QList<ResolvedScannerConstPtr> scanners;
    QList<GroupPtr> groups;
    QList<ArtifactPropertiesPtr> artifactProperties;
    QScopedPointer<ProductBuildData> buildData;

    mutable QProcessEnvironment buildEnvironment; // must not be saved
    mutable QProcessEnvironment runEnvironment; // must not be saved

    void accept(BuildGraphVisitor *visitor) const;
    QList<SourceArtifactPtr> allFiles() const;
    QList<SourceArtifactPtr> allEnabledFiles() const;
    FileTags fileTagsForFileName(const QString &fileName) const;
    void setupBuildEnvironment(ScriptEngine *scriptEngine, const QProcessEnvironment &env) const;
    void setupRunEnvironment(ScriptEngine *scriptEngine, const QProcessEnvironment &env) const;

    void registerArtifactWithChangedInputs(Artifact *artifact);
    void unregisterArtifactWithChangedInputs(Artifact *artifact);
    void unmarkForReapplication(const RuleConstPtr &rule);
    bool isMarkedForReapplication(const RuleConstPtr &rule) const;
    ArtifactSet lookupArtifactsByFileTag(const FileTag &tag) const;
    ArtifactSet targetArtifacts() const;

    TopLevelProject *topLevelProject() const;

    static QString uniqueName(const QString &name, const QString &profile);
    QString uniqueName() const;

    QStringList generatedFiles(const QString &baseFile, const FileTags &tags) const;

    static QString deriveBuildDirectoryName(const QString &name, const QString &profile);
    QString buildDirectory() const;

    bool isInParentProject(const ResolvedProductConstPtr &other) const;
    bool builtByDefault() const;

    void cacheExecutablePath(const QString &origFilePath, const QString &fullFilePath);
    QString cachedExecutablePath(const QString &origFilePath) const;

private:
    ResolvedProduct();

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

    QHash<QString, QString> m_executablePathCache;
    mutable QMutex m_executablePathCacheLock;
};

class ResolvedProject : public PersistentObject
{
public:
    static ResolvedProjectPtr create() { return ResolvedProjectPtr(new ResolvedProject); }

    QString name;
    CodeLocation location;
    bool enabled;
    QList<ResolvedProductPtr> products;
    QList<ResolvedProjectPtr> subProjects;
    WeakPointer<ResolvedProject> parentProject;

    void accept(BuildGraphVisitor *visitor) const;

    void setProjectProperties(const QVariantMap &config) { m_projectProperties = config; }
    const QVariantMap &projectProperties() const { return m_projectProperties; }

    TopLevelProject *topLevelProject();
    QList<ResolvedProjectPtr> allSubProjects() const;
    QList<ResolvedProductPtr> allProducts() const;

protected:
    ResolvedProject();

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
private:
    QVariantMap m_projectProperties;
    TopLevelProject *m_topLevelProject;
};

class TopLevelProject : public ResolvedProject
{
    friend class BuildGraphLoader;
public:
    ~TopLevelProject();

    static TopLevelProjectPtr create() { return TopLevelProjectPtr(new TopLevelProject); }

    static QString deriveId(const QString &profile, const QVariantMap &config);
    static QString deriveBuildDirectory(const QString &buildRoot, const QString &id);

    QString buildDirectory; // Not saved
    QProcessEnvironment environment;
    QHash<QString, QString> usedEnvironment; // Environment variables requested by the project while resolving.
    QHash<QString, QString> canonicalFilePathResults; // Results of calls to "File.canonicalFilePath()."
    QHash<QString, bool> fileExistsResults; // Results of calls to "File.exists()".
    QHash<QString, FileTime> fileLastModifiedResults; // Results of calls to "File.lastModified()".
    QScopedPointer<ProjectBuildData> buildData;
    BuildGraphLocker *bgLocker; // This holds the system-wide build graph file lock.
    bool locked; // This is the API-level lock for the project instance.

    QSet<QString> buildSystemFiles;
    FileTime lastResolveTime;

    void setBuildConfiguration(const QVariantMap &config);
    const QVariantMap &buildConfiguration() const { return m_buildConfiguration; }
    QString id() const { return m_id; }
    QString profile() const;
    QVariantMap profileConfigs;

    QString buildGraphFilePath() const;
    void store(const Logger &logger) const;

private:
    TopLevelProject();

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

    QString m_id;
    QVariantMap m_buildConfiguration;
};

bool artifactPropertyListsAreEqual(const QList<ArtifactPropertiesPtr> &l1,
                                   const QList<ArtifactPropertiesPtr> &l2);

} // namespace Internal
} // namespace qbs

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(qbs::Internal::JsImport, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(qbs::Internal::RuleArtifact::Binding, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

#endif // QBS_LANGUAGE_H
