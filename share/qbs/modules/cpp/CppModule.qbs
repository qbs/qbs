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

// base for Cpp modules
import qbs.ModUtils

Module {
    condition: false
    property int compilerVersionMajor
    property int compilerVersionMinor
    property int compilerVersionPatch
    property string warningLevel : 'all' // 'none', 'all'
    property bool treatWarningsAsErrors : false
    property string architecture: qbs.architecture
    property string optimization: qbs.optimization
    property bool debugInformation: qbs.debugInformation
    property bool separateDebugInformation: false
    property pathList prefixHeaders
    property path precompiledHeader
    property path cPrecompiledHeader: precompiledHeader
    property path cxxPrecompiledHeader: precompiledHeader
    // ### default to undefined on non-Apple platforms for now - QBS-346
    property path objcPrecompiledHeader: qbs.targetOS.contains("darwin") ? precompiledHeader : undefined
    property path objcxxPrecompiledHeader: qbs.targetOS.contains("darwin") ? precompiledHeader : undefined
    property path precompiledHeaderDir: product.buildDirectory
    property stringList defines
    property stringList platformDefines: qbs.enableDebugCode ? [] : ["NDEBUG"]
    property stringList compilerDefines
    PropertyOptions {
        name: "compilerDefines"
        description: "preprocessor macros that are defined when using this particular compiler"
    }

    property string windowsApiCharacterSet

    property string minimumWindowsVersion
    PropertyOptions {
        name: "minimumWindowsVersion"
        description: "A version number in the format [major].[minor] indicating the earliest \
                        version of Windows that the product should run on. Defines WINVER, \
                        _WIN32_WINNT, and _WIN32_WINDOWS, and applies a version number to the \
                        linker flags /SUBSYSTEM and /OSVERSION for MSVC or \
                        -Wl,--major-subsystem-version, -Wl,--minor-subsystem-version, \
                        -Wl,--major-os-version and -Wl,--minor-os-version for MinGW. \
                        If undefined, compiler defaults will be used."
    }

    property string minimumOsxVersion
    PropertyOptions {
        name: "minimumOsxVersion"
        description: "a version number in the format [major].[minor] indicating the earliest \
                        version of OS X that the product should run on. passes -mmacosx-version-min=<version> \
                        to the compiler. if undefined, compiler defaults will be used."
    }

    property string minimumIosVersion
    PropertyOptions {
        name: "minimumIosVersion"
        description: "a version number in the format [major].[minor] indicating the earliest \
                        version of iOS that the product should run on. passes -miphoneos-version-min=<version> \
                        to the compiler. if undefined, compiler defaults will be used."
    }

    property string minimumAndroidVersion
    PropertyOptions {
        name: "minimumAndroidVersion"
        description: "a version number in the format [major].[minor] indicating the earliest \
                        version of Android that the product should run on. this value is converted into an SDK \
                        version which is then written to AndroidManifest.xml."
    }

    property string maximumAndroidVersion
    PropertyOptions {
        name: "maximumAndroidVersion"
        description: "a version number in the format [major].[minor] indicating the latest \
                        version of Android that the product should run on. this value is converted into an SDK \
                        version which is then written to AndroidManifest.xml. if undefined, no upper limit will \
                        be set."
    }

    property string installNamePrefix
    PropertyOptions {
        name: "installNamePrefix"
        description: "The prefix for the internal install name (LC_ID_DYLIB) of a dynamic library \
                      on Darwin (OS X and iOS)."
    }

    property pathList includePaths
    property pathList systemIncludePaths
    property pathList libraryPaths
    property pathList frameworkPaths
    property pathList systemFrameworkPaths
    property pathList linkerScripts
    property string compilerName
    property string compilerPath: compilerName
    property var compilerPathByLanguage
    property stringList compilerWrapper
    property string linkerName
    property string linkerPath: linkerName
    property string staticLibraryPrefix
    property string dynamicLibraryPrefix
    property string loadableModulePrefix
    property string executablePrefix
    property string staticLibrarySuffix
    property string dynamicLibrarySuffix
    property string loadableModuleSuffix
    property string executableSuffix
    property string debugInfoSuffix
    property bool createSymlinks: true
    property stringList dynamicLibraries // list of names, will be linked with -lname
    property stringList staticLibraries // list of static library files
    property stringList frameworks // list of frameworks, will be linked with '-framework <name>'
    property stringList weakFrameworks // list of weakly-linked frameworks, will be linked with '-weak_framework <name>'
    property stringList rpaths
    property bool useRPaths: true

    property stringList cppFlags
    PropertyOptions {
        name: "cppFlags"
        description: "additional flags for the C preprocessor"
    }

    property stringList cFlags
    PropertyOptions {
        name: "cFlags"
        description: "additional flags for the C compiler"
    }

    property stringList cxxFlags
    PropertyOptions {
        name: "cxxFlags"
        description: "additional flags for the C++ compiler"
    }

    property stringList objcFlags
    PropertyOptions {
        name: "objcFlags"
        description: "additional flags for the Objective-C compiler"
    }

    property stringList objcxxFlags
    PropertyOptions {
        name: "objcxxFlags"
        description: "additional flags for the Objective-C++ compiler"
    }
    property stringList commonCompilerFlags
    PropertyOptions {
        name: "commonCompilerFlags"
        description: "flags added to all compilation independently of the language"
    }

    property stringList linkerFlags
    PropertyOptions {
        name: "linkerFlags"
        description: "additional linker flags"
    }

    property bool positionIndependentCode
    PropertyOptions {
        name: "positionIndependentCode"
        description: "generate position independent code"
    }

    property string entryPoint
    PropertyOptions {
        name: "entryPoint"
        description: "entry point symbol for an executable or dynamic library"
    }

    property string runtimeLibrary
    PropertyOptions {
        name: "runtimeLibrary"
        description: "determine which runtime library to use"
        allowedValues: ['static', 'dynamic']
    }

    property string visibility: 'default'
    PropertyOptions {
        name: "visibility"
        description: "export symbols visibility level"
        allowedValues: ['default', 'hidden', 'hiddenInlines', 'minimal']
    }

    property string cLanguageVersion
    PropertyOptions {
        name: "cLanguageVersion"
        allowedValues: ["c89", "c99", "c11"]
        description: "The version of the C standard with which the code must comply."
    }

    property string cxxLanguageVersion
    PropertyOptions {
        name: "cxxLanguageVersion"
        allowedValues: ["c++98", "c++11", "c++14"]
        description: "The version of the C++ standard with which the code must comply."
    }

    property string cxxStandardLibrary
    PropertyOptions {
        name: "cxxStandardLibrary"
        allowedValues: ["libstdc++", "libc++"]
        description: "version of the C++ standard library to use"
    }

    // Platform properties. Those are intended to be set by the toolchain setup
    // and are prepended to the corresponding user properties.
    property stringList platformCommonCompilerFlags
    property stringList platformCFlags
    property stringList platformCxxFlags
    property stringList platformObjcFlags
    property stringList platformObjcxxFlags
    property stringList platformLinkerFlags

    // OS X and iOS properties
    property bool buildIpa: !qbs.targetOS.contains('ios-simulator')

    property string signingIdentity
    property path provisioningProfile

    property string xcodeSdkName
    property string xcodeSdkVersion

    property bool automaticReferenceCounting
    PropertyOptions {
        name: "automaticReferenceCounting"
        description: "whether to enable Automatic Reference Counting (ARC) for Objective-C"
    }

    property bool requireAppExtensionSafeApi
    PropertyOptions {
        name: "requireAppExtensionSafeApi"
        description: "whether to require app-extension-safe APIs only"
    }

    property bool allowUnresolvedSymbols: false

    FileTagger {
        patterns: ["*.c"]
        fileTags: ["c"]
    }

    FileTagger {
        patterns: ["*.C", "*.cpp", "*.cxx", "*.c++", "*.cc"]
        fileTags: ["cpp"]
    }

    FileTagger {
        patterns: ["*.m"]
        fileTags: ["objc"]
    }

    FileTagger {
        patterns: ["*.mm"]
        fileTags: ["objcpp"]
    }

    FileTagger {
        patterns: ["*.h", "*.H", "*.hpp", "*.hxx", "*.h++"]
        fileTags: ["hpp"]
    }

    validate: {
        var validator = new ModUtils.PropertyValidator("cpp");
        validator.addCustomValidator("architecture", architecture, function (value) {
            return !architecture || architecture === canonicalArchitecture(architecture);
        }, "'" + architecture + "' is invalid. You must use the canonical name '" +
        canonicalArchitecture(architecture) + "'");
        validator.validate();
    }

    setupRunEnvironment: {
        var env = qbs.commonRunEnvironment;
        for (var i in env) {
            var v = new ModUtils.EnvironmentVariable(i, qbs.pathListSeparator,
                                                     qbs.hostOS.contains("windows"));
            v.value = env[i];
            v.set();
        }
    }
}
