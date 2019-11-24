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

#ifndef QBS_LANGUAGE_H
#define QBS_LANGUAGE_H

#include "filetags.h"
#include "forward_decls.h"
#include "jsimports.h"
#include "moduleproviderinfo.h"
#include "propertydeclaration.h"
#include "resolvedfilecontext.h"

#include <buildgraph/forward_decls.h>
#include <tools/codelocation.h>
#include <tools/filetime.h>
#include <tools/joblimits.h>
#include <tools/persistence.h>
#include <tools/set.h>
#include <tools/weakpointer.h>

#include <QtCore/qdatastream.h>
#include <QtCore/qhash.h>
#include <QtCore/qprocess.h>
#include <QtCore/qregexp.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

#include <QtScript/qscriptvalue.h>

#include <memory>
#include <mutex>
#include <vector>

QT_BEGIN_NAMESPACE
class QScriptEngine;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {
class BuildGraphLocker;
class BuildGraphLoader;
class BuildGraphVisitor;

class FileTagger
{
public:
    static FileTaggerPtr create() { return FileTaggerPtr(new FileTagger); }
    static FileTaggerPtr create(const QStringList &patterns, const FileTags &fileTags,
                                int priority) {
        return FileTaggerPtr(new FileTagger(patterns, fileTags, priority));
    }

    const QList<QRegExp> &patterns() const { return m_patterns; }
    const FileTags &fileTags() const { return m_fileTags; }
    int priority() const { return m_priority; }

    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(m_patterns, m_fileTags, m_priority);
    }

private:
    FileTagger(const QStringList &patterns, FileTags fileTags, int priority);
    FileTagger() = default;

    void setPatterns(const QStringList &patterns);

    QList<QRegExp> m_patterns;
    FileTags m_fileTags;
    int m_priority = 0;
};

class Probe
{
public:
    static ProbePtr create() { return ProbePtr(new Probe); }
    static ProbeConstPtr create(const QString &globalId,
                                const CodeLocation &location,
                                bool condition,
                                const QString &configureScript,
                                const QVariantMap &properties,
                                const QVariantMap &initialProperties,
                                const std::vector<QString> &importedFilesUsed)
    {
        return ProbeConstPtr(new Probe(globalId, location, condition, configureScript, properties,
                                       initialProperties, importedFilesUsed));
    }

    const QString &globalId() const { return m_globalId; }
    bool condition() const { return m_condition; }
    const QString &configureScript() const { return m_configureScript; }
    const QVariantMap &properties() const { return m_properties; }
    const QVariantMap &initialProperties() const { return m_initialProperties; }
    const std::vector<QString> &importedFilesUsed() const { return m_importedFilesUsed; }
    bool needsReconfigure(const FileTime &referenceTime) const;

    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(m_globalId, m_location, m_condition, m_configureScript,
                                     m_properties, m_initialProperties, m_importedFilesUsed);
    }

private:
    Probe() = default;
    Probe(QString globalId,
          const CodeLocation &location,
          bool condition,
          QString configureScript,
          QVariantMap properties,
          QVariantMap initialProperties,
          std::vector<QString> importedFilesUsed)
        : m_globalId(std::move(globalId))
        , m_location(location)
        , m_configureScript(std::move(configureScript))
        , m_properties(std::move(properties))
        , m_initialProperties(std::move(initialProperties))
        , m_importedFilesUsed(std::move(importedFilesUsed))
        , m_condition(condition)
    {}

    QString m_globalId;
    CodeLocation m_location;
    QString m_configureScript;
    QVariantMap m_properties;
    QVariantMap m_initialProperties;
    std::vector<QString> m_importedFilesUsed;
    bool m_condition = false;
};

class RuleArtifact
{
public:
    static RuleArtifactPtr create() { return RuleArtifactPtr(new RuleArtifact); }

    QString filePath;
    FileTags fileTags;
    bool alwaysUpdated;
    CodeLocation location;
    CodeLocation filePathLocation;

    class Binding
    {
    public:
        QStringList name;
        QString code;
        CodeLocation location;

        template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
        {
            pool.serializationOp<opType>(name, code, location);
        }

        bool operator<(const Binding &other) const
        {
            if (name == other.name) {
                if (code == other.code)
                    return location < other.location;
                return code < other.code;
            }
            return name < other.name;
        }
    };

    std::vector<Binding> bindings;

    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(filePath, fileTags, alwaysUpdated, location, filePathLocation,
                                     bindings);
    }

private:
    RuleArtifact()
        : alwaysUpdated(true)
    {}
};
uint qHash(const RuleArtifact::Binding &b);
bool operator==(const RuleArtifact::Binding &b1, const RuleArtifact::Binding &b2);
inline bool operator!=(const RuleArtifact::Binding &b1, const RuleArtifact::Binding &b2) {
    return !(b1 == b2);
}
bool operator==(const RuleArtifact &a1, const RuleArtifact &a2);
inline bool operator!=(const RuleArtifact &a1, const RuleArtifact &a2) { return !(a1 == a2); }

class SourceArtifactInternal
{
public:
    static SourceArtifactPtr create() { return SourceArtifactPtr(new SourceArtifactInternal); }

    bool isTargetOfModule() const { return !targetOfModule.isEmpty(); }

    QString absoluteFilePath;
    FileTags fileTags;
    bool overrideFileTags;
    QString targetOfModule;
    PropertyMapPtr properties;

    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(absoluteFilePath, fileTags, overrideFileTags, properties,
                                     targetOfModule);
    }

private:
    SourceArtifactInternal() : overrideFileTags(true) {}
};
bool operator==(const SourceArtifactInternal &sa1, const SourceArtifactInternal &sa2);
inline bool operator!=(const SourceArtifactInternal &sa1, const SourceArtifactInternal &sa2) {
    return !(sa1 == sa2);
}

class SourceWildCards
{
public:
    Set<QString> expandPatterns(const GroupConstPtr &group, const QString &baseDir,
                                 const QString &buildDir);

    const ResolvedGroup *group = nullptr;       // The owning group.
    QStringList patterns;
    QStringList excludePatterns;
    std::vector<std::pair<QString, FileTime>> dirTimeStamps;
    std::vector<SourceArtifactPtr> files;

    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(patterns, excludePatterns, dirTimeStamps, files);
    }

private:
    Set<QString> expandPatterns(const GroupConstPtr &group, const QStringList &patterns,
                                 const QString &baseDir, const QString &buildDir);
    void expandPatterns(Set<QString> &result, const GroupConstPtr &group,
                        const QStringList &parts, const QString &baseDir,
                        const QString &buildDir);
};

class QBS_AUTOTEST_EXPORT ResolvedGroup
{
public:
    static GroupPtr create() { return GroupPtr(new ResolvedGroup); }

    CodeLocation location;

    QString name;
    bool enabled = true;
    QString prefix;
    std::vector<SourceArtifactPtr> files;
    std::unique_ptr<SourceWildCards> wildcards;
    PropertyMapPtr properties;
    FileTags fileTags;
    QString targetOfModule;
    bool overrideTags = false;

    std::vector<SourceArtifactPtr> allFiles() const;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool);

private:
    template<PersistentPool::OpType opType> void serializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(name, enabled, location, prefix, files, wildcards, properties,
                                     fileTags, targetOfModule, overrideTags);
    }
};

class ScriptFunction
{
public:
    static ScriptFunctionPtr create() { return ScriptFunctionPtr(new ScriptFunction); }

    ~ScriptFunction();

    QString sourceCode;
    CodeLocation location;
    ResolvedFileContextConstPtr fileContext;

    bool isValid() const;

    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(sourceCode, location, fileContext);
    }

private:
    ScriptFunction();
};

bool operator==(const ScriptFunction &a, const ScriptFunction &b);
inline bool operator!=(const ScriptFunction &a, const ScriptFunction &b) { return !(a == b); }

bool operator==(const PrivateScriptFunction &a, const PrivateScriptFunction &b);

class PrivateScriptFunction
{
    friend bool operator==(const PrivateScriptFunction &a, const PrivateScriptFunction &b);
public:
    void initialize(const ScriptFunctionPtr &sharedData) { m_sharedData = sharedData; }
    mutable QScriptValue scriptFunction; // not stored

    QString &sourceCode() const { return m_sharedData->sourceCode; }
    CodeLocation &location()  const { return m_sharedData->location; }
    ResolvedFileContextConstPtr &fileContext() const { return m_sharedData->fileContext; }
    bool isValid() const { return m_sharedData->isValid(); }

    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(m_sharedData);
    }

private:
    ScriptFunctionPtr m_sharedData;
};

bool operator==(const PrivateScriptFunction &a, const PrivateScriptFunction &b);
inline bool operator!=(const PrivateScriptFunction &a, const PrivateScriptFunction &b)
{
    return !(a == b);
}

class ResolvedModule
{
public:
    static ResolvedModulePtr create() { return ResolvedModulePtr(new ResolvedModule); }

    QString name;
    QStringList moduleDependencies;
    PrivateScriptFunction setupBuildEnvironmentScript;
    PrivateScriptFunction setupRunEnvironmentScript;
    ResolvedProduct *product = nullptr;
    bool isProduct = false;

    static QStringList argumentNamesForSetupBuildEnv();
    static QStringList argumentNamesForSetupRunEnv();

    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(name, moduleDependencies, setupBuildEnvironmentScript,
                                     setupRunEnvironmentScript, isProduct);
    }

private:
    ResolvedModule() = default;
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
class Rule
{
public:
    static RulePtr create() { return RulePtr(new Rule); }
    RulePtr clone() const;

    ResolvedProduct *product = nullptr;         // The owning product.
    ResolvedModuleConstPtr module;
    QString name;
    PrivateScriptFunction prepareScript;
    FileTags outputFileTags;                    // unused, if artifacts is non-empty
    PrivateScriptFunction outputArtifactsScript;    // unused, if artifacts is non-empty
    FileTags inputs;
    FileTags auxiliaryInputs;
    FileTags excludedInputs;
    FileTags inputsFromDependencies;
    FileTags explicitlyDependsOn;
    FileTags explicitlyDependsOnFromDependencies;
    bool multiplex = false;
    bool requiresInputs = false;
    std::vector<RuleArtifactPtr> artifacts;     // unused, if outputFileTags/outputArtifactsScript is non-empty
    bool alwaysRun = false;

    // members that we don't need to save
    int ruleGraphId = -1;

    static QStringList argumentNamesForOutputArtifacts();
    static QStringList argumentNamesForPrepare();

    QString toString() const;
    FileTags staticOutputFileTags() const;
    FileTags collectedOutputFileTags() const;
    bool isDynamic() const;
    bool declaresInputs() const;

    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(name, prepareScript, outputArtifactsScript, module, inputs,
                                     outputFileTags, auxiliaryInputs, excludedInputs,
                                     inputsFromDependencies, explicitlyDependsOn,
                                     explicitlyDependsOnFromDependencies, multiplex,
                                     requiresInputs, alwaysRun, artifacts);
    }
private:
    Rule() = default;
};
bool operator==(const Rule &r1, const Rule &r2);
inline bool operator!=(const Rule &r1, const Rule &r2) { return !(r1 == r2); }
bool ruleListsAreEqual(const std::vector<RulePtr> &l1, const std::vector<RulePtr> &l2);

class ResolvedScanner
{
public:
    static ResolvedScannerPtr create() { return ResolvedScannerPtr(new ResolvedScanner); }

    ResolvedModuleConstPtr module;
    FileTags inputs;
    bool recursive;
    PrivateScriptFunction searchPathsScript;
    PrivateScriptFunction scanScript;

    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(module, inputs, recursive, searchPathsScript, scanScript);
    }

private:
    ResolvedScanner() :
        recursive(false)
    {}
};

class ExportedProperty
{
public:
    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(fullName, type, sourceCode, isBuiltin);
    }

    QString fullName;
    PropertyDeclaration::Type type = PropertyDeclaration::Type::UnknownType;
    QString sourceCode;
    bool isBuiltin = false;
};

bool operator==(const ExportedProperty &p1, const ExportedProperty &p2);
inline bool operator!=(const ExportedProperty &p1, const ExportedProperty &p2)
{
    return !(p1 == p2);
}

class ExportedItem
{
public:
    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(name, properties, children);
    }

    static ExportedItemPtr create() { return std::make_shared<ExportedItem>(); }

    QString name;
    std::vector<ExportedProperty> properties;
    std::vector<ExportedItemPtr> children;
};

bool equals(const std::vector<ExportedItemPtr> &l1, const std::vector<ExportedItemPtr> &l2);
bool operator==(const ExportedItem &i1, const ExportedItem &i2);
inline bool operator!=(const ExportedItem &i1, const ExportedItem &i2) { return !(i1 == i2); }

class ExportedModuleDependency
{
public:
    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(name, moduleProperties);
    };

    QString name;
    QVariantMap moduleProperties;
};

bool operator==(const ExportedModuleDependency &d1, const ExportedModuleDependency &d2);
inline bool operator!=(const ExportedModuleDependency &d1, const ExportedModuleDependency &d2)
{
    return !(d1 == d2);
}

class ExportedModule
{
public:
    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(propertyValues, modulePropertyValues, children,
                                     productDependencies, moduleDependencies, m_properties,
                                     dependencyParameters, importStatements);
    };

    QVariantMap propertyValues;
    QVariantMap modulePropertyValues;
    std::vector<ExportedItemPtr> children;
    std::vector<QString> productDependencies;
    std::vector<ExportedModuleDependency> moduleDependencies;
    std::vector<ExportedProperty> m_properties;
    QMap<ResolvedProductConstPtr, QVariantMap> dependencyParameters;
    QStringList importStatements;
};

bool operator==(const ExportedModule &m1, const ExportedModule &m2);
inline bool operator!=(const ExportedModule &m1, const ExportedModule &m2) { return !(m1 == m2); }

class TopLevelProject;
class ScriptEngine;

class QBS_AUTOTEST_EXPORT ResolvedProduct
{
public:
    static ResolvedProductPtr create() { return ResolvedProductPtr(new ResolvedProduct); }

    ~ResolvedProduct();

    bool enabled;
    FileTags fileTags;
    QString name;
    QString targetName;
    QString multiplexConfigurationId;
    QString sourceDirectory;
    QString destinationDirectory;
    CodeLocation location;
    WeakPointer<ResolvedProject> project;
    QVariantMap productProperties;
    PropertyMapPtr moduleProperties;
    std::vector<RulePtr> rules;
    std::vector<ResolvedProductPtr> dependencies;
    QHash<ResolvedProductConstPtr, QVariantMap> dependencyParameters;
    std::vector<FileTaggerConstPtr> fileTaggers;
    JobLimits jobLimits;
    std::vector<ResolvedModulePtr> modules;
    QHash<ResolvedModuleConstPtr, QVariantMap> moduleParameters;
    std::vector<ResolvedScannerConstPtr> scanners;
    std::vector<GroupPtr> groups;
    std::vector<ProbeConstPtr> probes;
    std::vector<ArtifactPropertiesPtr> artifactProperties;
    QStringList missingSourceFiles;
    std::unique_ptr<ProductBuildData> buildData;

    ExportedModule exportedModule;

    QProcessEnvironment buildEnvironment; // must not be saved
    QProcessEnvironment runEnvironment; // must not be saved

    void accept(BuildGraphVisitor *visitor) const;
    std::vector<SourceArtifactPtr> allFiles() const;
    std::vector<SourceArtifactPtr> allEnabledFiles() const;
    FileTags fileTagsForFileName(const QString &fileName) const;

    ArtifactSet lookupArtifactsByFileTag(const FileTag &tag) const;
    ArtifactSet lookupArtifactsByFileTags(const FileTags &tags) const;
    ArtifactSet targetArtifacts() const;

    TopLevelProject *topLevelProject() const;

    static QString uniqueName(const QString &name,
                              const QString &multiplexConfigurationId);
    QString uniqueName() const;
    static QString fullDisplayName(const QString &name, const QString &multiplexConfigurationId);
    QString fullDisplayName() const;
    QString profile() const;

    QStringList generatedFiles(const QString &baseFile, bool recursive, const FileTags &tags) const;

    static QString deriveBuildDirectoryName(const QString &name,
                                            const QString &multiplexConfigurationId);
    QString buildDirectory() const;

    bool isInParentProject(const ResolvedProductConstPtr &other) const;
    bool builtByDefault() const;

    void cacheExecutablePath(const QString &origFilePath, const QString &fullFilePath);
    QString cachedExecutablePath(const QString &origFilePath) const;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool);

private:
    ResolvedProduct();

    template<PersistentPool::OpType opType> void serializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(enabled, fileTags, name, multiplexConfigurationId,
                                     targetName, sourceDirectory, destinationDirectory,
                                     missingSourceFiles, location, productProperties,
                                     moduleProperties, rules, dependencies, dependencyParameters,
                                     fileTaggers, modules, moduleParameters, scanners, groups,
                                     artifactProperties, probes, exportedModule, buildData,
                                     jobLimits);
    }

    QHash<QString, QString> m_executablePathCache;
    mutable std::mutex m_executablePathCacheLock;
};

class QBS_AUTOTEST_EXPORT ResolvedProject
{
public:
    virtual ~ResolvedProject();
    static ResolvedProjectPtr create() { return ResolvedProjectPtr(new ResolvedProject); }

    QString name;
    CodeLocation location;
    bool enabled;
    std::vector<ResolvedProductPtr> products;
    std::vector<ResolvedProjectPtr> subProjects;
    WeakPointer<ResolvedProject> parentProject;

    void accept(BuildGraphVisitor *visitor) const;

    void setProjectProperties(const QVariantMap &config) { m_projectProperties = config; }
    const QVariantMap &projectProperties() const { return m_projectProperties; }

    TopLevelProject *topLevelProject();
    std::vector<ResolvedProjectPtr> allSubProjects() const;
    std::vector<ResolvedProductPtr> allProducts() const;

    virtual void load(PersistentPool &pool);
    virtual void store(PersistentPool &pool);

protected:
    ResolvedProject();

private:
    template<PersistentPool::OpType opType> void serializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(name, location, enabled, products, subProjects,
                                     m_projectProperties);
    }

    QVariantMap m_projectProperties;
    TopLevelProject *m_topLevelProject;
};

class QBS_AUTOTEST_EXPORT TopLevelProject : public ResolvedProject
{
    friend class BuildGraphLoader;
public:
    ~TopLevelProject() override;

    static TopLevelProjectPtr create() { return TopLevelProjectPtr(new TopLevelProject); }

    static QString deriveId(const QVariantMap &config);
    static QString deriveBuildDirectory(const QString &buildRoot, const QString &id);

    QString buildDirectory; // Not saved
    QProcessEnvironment environment;
    std::vector<ProbeConstPtr> probes;
    ModuleProviderInfoList moduleProviderInfo;

    QHash<QString, QString> canonicalFilePathResults; // Results of calls to "File.canonicalFilePath()."
    QHash<QString, bool> fileExistsResults; // Results of calls to "File.exists()".
    QHash<std::pair<QString, quint32>, QStringList> directoryEntriesResults; // Results of calls to "File.directoryEntries()".
    QHash<QString, FileTime> fileLastModifiedResults; // Results of calls to "File.lastModified()".
    std::unique_ptr<ProjectBuildData> buildData;
    BuildGraphLocker *bgLocker; // This holds the system-wide build graph file lock.
    bool locked; // This is the API-level lock for the project instance.

    Set<QString> buildSystemFiles;
    FileTime lastStartResolveTime;
    FileTime lastEndResolveTime;
    QList<ErrorInfo> warningsEncountered;

    void setBuildConfiguration(const QVariantMap &config);
    const QVariantMap &buildConfiguration() const { return m_buildConfiguration; }
    QString id() const { return m_id; }
    QString profile() const;
    void makeModuleProvidersNonTransient();

    QVariantMap profileConfigs;
    QVariantMap overriddenValues;

    QString buildGraphFilePath() const;
    void store(Logger logger);

private:
    TopLevelProject();

    template<PersistentPool::OpType opType> void serializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(m_id, canonicalFilePathResults, fileExistsResults,
                                     directoryEntriesResults, fileLastModifiedResults, environment,
                                     probes, profileConfigs, overriddenValues, buildSystemFiles,
                                     lastStartResolveTime, lastEndResolveTime, warningsEncountered,
                                     buildData, moduleProviderInfo);
    }
    void load(PersistentPool &pool) override;
    void store(PersistentPool &pool) override;

    void cleanupModuleProviderOutput();

    QString m_id;
    QVariantMap m_buildConfiguration;
};

bool artifactPropertyListsAreEqual(const std::vector<ArtifactPropertiesPtr> &l1,
                                   const std::vector<ArtifactPropertiesPtr> &l2);

QString multiplexIdToString(const QString &id);

} // namespace Internal
} // namespace qbs

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(qbs::Internal::JsImport, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(qbs::Internal::RuleArtifact::Binding, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

#endif // QBS_LANGUAGE_H
