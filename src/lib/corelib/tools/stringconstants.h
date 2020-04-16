/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef QBS_STRINGCONSTANTS_H
#define QBS_STRINGCONSTANTS_H

#include <QtCore/qstringlist.h>

#define QBS_CONSTANT(type, name, value)                   \
    static const type &name() {                           \
        static const type var{QLatin1String(value)};      \
        return var;                                       \
    }
#define QBS_STRING_CONSTANT(name, value) QBS_CONSTANT(QString, name, value)
#define QBS_STRINGLIST_CONSTANT(name, value) QBS_CONSTANT(QStringList, name, value)

namespace qbs {
namespace Internal {

class StringConstants
{
public:
    static const QString &cppModule() { return cpp(); }
    static const QString &qbsModule() { return qbs(); }

    QBS_STRING_CONSTANT(aggregateProperty, "aggregate")
    QBS_STRING_CONSTANT(additionalProductTypesProperty, "additionalProductTypes")
    QBS_STRING_CONSTANT(allowedValuesProperty, "allowedValues")
    QBS_STRING_CONSTANT(alwaysUpdatedProperty, "alwaysUpdated")
    QBS_STRING_CONSTANT(alwaysRunProperty, "alwaysRun")
    QBS_STRING_CONSTANT(artifactsProperty, "artifacts")
    QBS_STRING_CONSTANT(auxiliaryInputsProperty, "auxiliaryInputs")
    QBS_STRING_CONSTANT(baseNameProperty, "baseName")
    QBS_STRING_CONSTANT(baseProfileProperty, "baseProfile")
    QBS_STRING_CONSTANT(buildDirectoryProperty, "buildDirectory")
    QBS_STRING_CONSTANT(buildDirectoryKey, "build-directory")
    QBS_STRING_CONSTANT(builtByDefaultProperty, "builtByDefault")
    QBS_STRING_CONSTANT(classNameProperty, "className")
    QBS_STRING_CONSTANT(completeBaseNameProperty, "completeBaseName")
    QBS_STRING_CONSTANT(conditionProperty, "condition")
    QBS_STRING_CONSTANT(configurationNameProperty, "configurationName")
    QBS_STRING_CONSTANT(configureProperty, "configure")
    QBS_STRING_CONSTANT(consoleApplicationProperty, "consoleApplication")
    QBS_STRING_CONSTANT(dependenciesProperty, "dependencies")
    QBS_STRING_CONSTANT(descriptionProperty, "description")
    QBS_STRING_CONSTANT(destinationDirProperty, "destinationDirectory")
    QBS_STRING_CONSTANT(excludeFilesProperty, "excludeFiles")
    QBS_STRING_CONSTANT(excludedAuxiliaryInputsProperty, "excludedAuxiliaryInputs")
    QBS_STRING_CONSTANT(excludedInputsProperty, "excludedInputs")
    static const QString &explicitlyDependsOnProperty() { return explicitlyDependsOn(); }
    static const QString &explicitlyDependsOnFromDependenciesProperty() {
        return explicitlyDependsOnFromDependencies();
    }
    QBS_STRING_CONSTANT(enableFallbackProperty, "enableFallback")
    static const QString &fileNameProperty() { return fileName(); }
    static const QString &filePathProperty() { return filePath(); }
    static const QString &filePathVar() { return filePath(); }
    QBS_STRING_CONSTANT(filePathKey, "file-path")
    QBS_STRING_CONSTANT(fileTagsFilterProperty, "fileTagsFilter")
    QBS_STRING_CONSTANT(fileTagsProperty, "fileTags")
    QBS_STRING_CONSTANT(filesProperty, "files")
    QBS_STRING_CONSTANT(filesAreTargetsProperty, "filesAreTargets")
    QBS_STRING_CONSTANT(foundProperty, "found")
    QBS_STRING_CONSTANT(fullDisplayNameKey, "full-display-name")
    QBS_STRING_CONSTANT(imports, "imports")
    static const QString &importsDir() { return imports(); }
    static const QString &importsProperty() { return imports(); }
    QBS_STRING_CONSTANT(inheritPropertiesProperty, "inheritProperties")
    static const QString &inputsProperty() { return inputs(); }
    QBS_STRING_CONSTANT(inputsFromDependenciesProperty, "inputsFromDependencies")
    static const QString &installProperty() { return install(); }
    QBS_STRING_CONSTANT(installRootProperty, "installRoot")
    QBS_STRING_CONSTANT(installPrefixProperty, "installPrefix")
    QBS_STRING_CONSTANT(installDirProperty, "installDir")
    QBS_STRING_CONSTANT(installSourceBaseProperty, "installSourceBase")
    QBS_STRING_CONSTANT(isEnabledKey, "is-enabled")
    QBS_STRING_CONSTANT(jobCountProperty, "jobCount")
    QBS_STRING_CONSTANT(jobPoolProperty, "jobPool")
    QBS_STRING_CONSTANT(lengthProperty, "length")
    QBS_STRING_CONSTANT(limitToSubProjectProperty, "limitToSubProject")
    QBS_STRING_CONSTANT(locationKey, "location")
    QBS_STRING_CONSTANT(messageKey, "message")
    QBS_STRING_CONSTANT(minimumQbsVersionProperty, "minimumQbsVersion")
    QBS_STRING_CONSTANT(moduleNameProperty, "moduleName")
    QBS_STRING_CONSTANT(modulePropertiesKey, "module-properties")
    QBS_STRING_CONSTANT(moduleProviders, "moduleProviders")
    QBS_STRING_CONSTANT(multiplexByQbsPropertiesProperty, "multiplexByQbsProperties")
    QBS_STRING_CONSTANT(multiplexConfigurationIdProperty, "multiplexConfigurationId")
    QBS_STRING_CONSTANT(multiplexConfigurationIdsProperty, "multiplexConfigurationIds")
    QBS_STRING_CONSTANT(multiplexProperty, "multiplex")
    QBS_STRING_CONSTANT(multiplexedProperty, "multiplexed")
    QBS_STRING_CONSTANT(multiplexedTypeProperty, "multiplexedType")
    QBS_STRING_CONSTANT(nameProperty, "name")
    QBS_STRING_CONSTANT(outputArtifactsProperty, "outputArtifacts")
    QBS_STRING_CONSTANT(outputFileTagsProperty, "outputFileTags")
    QBS_STRING_CONSTANT(overrideTagsProperty, "overrideTags")
    QBS_STRING_CONSTANT(overrideListPropertiesProperty, "overrideListProperties")
    QBS_STRING_CONSTANT(parametersProperty, "parameters")
    static const QString &pathProperty() { return path(); }
    QBS_STRING_CONSTANT(patternsProperty, "patterns")
    QBS_STRING_CONSTANT(prefixMappingProperty, "prefixMapping")
    QBS_STRING_CONSTANT(prefixProperty, "prefix")
    QBS_STRING_CONSTANT(prepareProperty, "prepare")
    QBS_STRING_CONSTANT(presentProperty, "present")
    QBS_STRING_CONSTANT(priorityProperty, "priority")
    QBS_STRING_CONSTANT(profileProperty, "profile")
    static const QString &profilesProperty() { return profiles(); }
    QBS_STRING_CONSTANT(productTypesProperty, "productTypes")
    QBS_STRING_CONSTANT(productsKey, "products")
    QBS_STRING_CONSTANT(qbsSearchPathsProperty, "qbsSearchPaths")
    QBS_STRING_CONSTANT(referencesProperty, "references")
    QBS_STRING_CONSTANT(recursiveProperty, "recursive")
    QBS_STRING_CONSTANT(requiredProperty, "required")
    QBS_STRING_CONSTANT(requiresInputsProperty, "requiresInputs")
    QBS_STRING_CONSTANT(removalVersionProperty, "removalVersion")
    QBS_STRING_CONSTANT(scanProperty, "scan")
    QBS_STRING_CONSTANT(searchPathsProperty, "searchPaths")
    QBS_STRING_CONSTANT(setupBuildEnvironmentProperty, "setupBuildEnvironment")
    QBS_STRING_CONSTANT(setupRunEnvironmentProperty, "setupRunEnvironment")
    QBS_STRING_CONSTANT(shadowProductPrefix, "__shadow__")
    QBS_STRING_CONSTANT(sourceCodeProperty, "sourceCode")
    QBS_STRING_CONSTANT(sourceDirectoryProperty, "sourceDirectory")
    QBS_STRING_CONSTANT(submodulesProperty, "submodules")
    QBS_STRING_CONSTANT(targetNameProperty, "targetName")
    static const QString &typeProperty() { return type(); }
    QBS_STRING_CONSTANT(type, "type")
    QBS_STRING_CONSTANT(validateProperty, "validate")
    QBS_STRING_CONSTANT(versionProperty, "version")
    QBS_STRING_CONSTANT(versionAtLeastProperty, "versionAtLeast")
    QBS_STRING_CONSTANT(versionBelowProperty, "versionBelow")

    QBS_STRING_CONSTANT(importScopeNamePropertyInternal, "_qbs_importScopeName")
    QBS_STRING_CONSTANT(modulePropertyInternal, "__module")
    QBS_STRING_CONSTANT(qbsSourceDirPropertyInternal, "_qbs_sourceDir")
    static const char *qbsProcEnvVarInternal() { return "_qbs_procenv"; }

    static const QString &projectPrefix() { return project(); }
    static const QString &productValue() { return product(); }

    QBS_STRING_CONSTANT(projectsOverridePrefix, "projects.")
    QBS_STRING_CONSTANT(productsOverridePrefix, "products.")

    QBS_STRING_CONSTANT(baseVar, "base")
    static const QString &explicitlyDependsOnVar() { return explicitlyDependsOn(); }
    QBS_STRING_CONSTANT(inputVar, "input")
    static const QString &inputsVar() { return inputs(); }
    QBS_STRING_CONSTANT(originalVar, "original")
    QBS_STRING_CONSTANT(outerVar, "outer")
    QBS_STRING_CONSTANT(outputVar, "output")
    QBS_STRING_CONSTANT(outputsVar, "outputs")
    static const QString &productVar() { return product(); }
    static const QString &projectVar() { return project(); }

    static const QString &filePathGlobalVar() { return filePath(); }
    static const QString &pathGlobalVar() { return path(); }

    static const QString &pathType() { return path(); }

    static const QString &fileInfoFileName() { return fileName(); }
    static const QString &fileInfoPath() { return path(); }

    static const QString &androidInstallCommand() { return install(); }
    static const QString &simctlInstallCommand() { return  install(); }

    static const QString &profilesSettingsKey() { return profiles(); }

    QBS_STRING_CONSTANT(emptyArrayValue, "[]")
    QBS_STRING_CONSTANT(falseValue, "false")
    QBS_STRING_CONSTANT(trueValue, "true")
    QBS_STRING_CONSTANT(undefinedValue, "undefined")

    QBS_STRING_CONSTANT(javaScriptCommandType, "JavaScriptCommand")
    QBS_STRING_CONSTANT(commandType, "Command")

    QBS_STRING_CONSTANT(pathEnvVar, "PATH")

    QBS_STRING_CONSTANT(dot, ".")
    QBS_STRING_CONSTANT(dotDot, "..")
    QBS_STRING_CONSTANT(slashDotDot, "/..")
    QBS_STRING_CONSTANT(star, "*")
    QBS_STRING_CONSTANT(tildeSlash, "~/")

    QBS_STRINGLIST_CONSTANT(qbsFileWildcards, "*.qbs")
    QBS_STRINGLIST_CONSTANT(jsFileWildcards, "*.js")

    static const QString &cppLang() { return cpp(); }

    QBS_STRING_CONSTANT(xcode, "xcode")

    QBS_STRING_CONSTANT(aarch64Arch, "aarch64")
    QBS_STRING_CONSTANT(amd64Arch, "amd64")
    QBS_STRING_CONSTANT(armArch, "arm")
    QBS_STRING_CONSTANT(arm64Arch, "arm64")
    QBS_STRING_CONSTANT(armv7Arch, "armv7")
    QBS_STRING_CONSTANT(i386Arch, "i386")
    QBS_STRING_CONSTANT(i586Arch, "i586")
    QBS_STRING_CONSTANT(mipsArch, "mips")
    QBS_STRING_CONSTANT(mips64Arch, "mips64")
    QBS_STRING_CONSTANT(powerPcArch, "powerpc")
    QBS_STRING_CONSTANT(ppcArch, "ppc")
    QBS_STRING_CONSTANT(ppc64Arch, "ppc64")
    QBS_STRING_CONSTANT(x86Arch, "x86")
    QBS_STRING_CONSTANT(x86_64Arch, "x86_64")

    QBS_STRING_CONSTANT(profilesSettingsPrefix, "profiles.")

private:
    QBS_STRING_CONSTANT(cpp, "cpp")
    QBS_STRING_CONSTANT(explicitlyDependsOn, "explicitlyDependsOn")
    QBS_STRING_CONSTANT(explicitlyDependsOnFromDependencies, "explicitlyDependsOnFromDependencies")
    QBS_STRING_CONSTANT(fileName, "fileName")
    QBS_STRING_CONSTANT(filePath, "filePath")
    QBS_STRING_CONSTANT(inputs, "inputs")
    QBS_STRING_CONSTANT(install, "install")
    QBS_STRING_CONSTANT(path, "path")
    QBS_STRING_CONSTANT(product, "product")
    QBS_STRING_CONSTANT(profiles, "profiles")
    QBS_STRING_CONSTANT(project, "project")
    QBS_STRING_CONSTANT(qbs, "qbs")
};

} // namespace Internal
} // namespace qbs

#endif // Include guard
