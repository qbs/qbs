// base for Cpp modules

Module {
    condition: false
    additionalProductFileTags: ["hpp"]  // to include all rules that generate hpp files

    property string warningLevel : 'all' // 'none', 'all'
    property bool treatWarningsAsErrors : false
    property string architecture: qbs.architecture
    property string optimization: qbs.optimization
    property bool debugInformation: qbs.debugInformation
    property path precompiledHeader
    property path precompiledHeaderDir: product.buildDirectory
    property var defines
    property var platformDefines: qbs.enableDebugCode ? [] : ["NDEBUG"]
    property var compilerDefines
    PropertyOptions {
        name: "compilerDefines"
        description: "preprocessor macros that are defined when using this particular compiler"
    }

    property string minimumWindowsVersion
    PropertyOptions {
        name: "minimumWindowsVersion"
        description: "a version number in the format [major].[minor] indicating the earliest \
                        version of Windows that the product should run on. defines WINVER, \
                        _WIN32_WINNT, and _WIN32_WINDOWS, and applies a version number to the \
                        linker flags /SUBSYSTEM and /OSVERSION (or -Wl,-subsystem and -Wl,-osversion). \
                        if undefined, compiler defaults will be used."
    }

    property string minimumMacVersion
    PropertyOptions {
        name: "minimumMacVersion"
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
        description: "The prefix for the internal install name (LC_ID_DYLIB) of an OS X dynamic\
                      library."
    }

    property pathList includePaths
    property pathList systemIncludePaths
    property pathList libraryPaths
    property pathList frameworkPaths
    property pathList systemFrameworkPaths
    property string compilerName
    property string compilerPath
    property string staticLibraryPrefix
    property string dynamicLibraryPrefix
    property string executablePrefix
    property string staticLibrarySuffix
    property string dynamicLibrarySuffix
    property string executableSuffix
    property var dynamicLibraries // list of names, will be linked with -lname
    property var staticLibraries // list of static library files
    property var frameworks // list of frameworks, will be linked with '-framework <name>'
    property var weakFrameworks // list of weakly-linked frameworks, will be linked with '-weak_framework <name>'
    property var rpaths

    property var cppFlags
    PropertyOptions {
        name: "cppFlags"
        description: "additional flags for the C preprocessor"
    }

    property var cFlags
    PropertyOptions {
        name: "cFlags"
        description: "additional flags for the C compiler"
    }

    property var cxxFlags
    PropertyOptions {
        name: "cxxFlags"
        description: "additional flags for the C++ compiler"
    }

    property var objcFlags
    PropertyOptions {
        name: "objcFlags"
        description: "additional flags for the Objective-C compiler"
    }

    property var objcxxFlags
    PropertyOptions {
        name: "objcxxFlags"
        description: "additional flags for the Objective-C++ compiler"
    }

    property var linkerFlags
    PropertyOptions {
        name: "linkerFlags"
        description: "additional linker flags"
    }

    property var positionIndependentCode
    PropertyOptions {
        name: "positionIndependentCode"
        description: "generate position independent code"
    }

    property string visibility: 'default' // 'default', 'hidden', 'hiddenInlines'
    PropertyOptions {
        name: "visibility"
        description: "export symbols visibility level"
        allowedValues: ['default', 'hidden', 'hiddenInlines']
    }

    // Platform properties. Those are intended to be set by the toolchain setup
    // and are prepended to the corresponding user properties.
    property var platformCFlags
    property var platformCxxFlags
    property var platformObjcFlags
    property var platformObjcxxFlags
    property var platformLinkerFlags

    FileTagger {
        pattern: "*.c"
        fileTags: ["c"]
    }

    FileTagger {
        pattern: "*.C"
        fileTags: ["c"]
    }

    FileTagger {
        pattern: "*.cpp"
        fileTags: ["cpp"]
    }

    FileTagger {
        pattern: "*.cxx"
        fileTags: ["cpp"]
    }

    FileTagger {
        pattern: "*.c++"
        fileTags: ["cpp"]
    }

    FileTagger {
        pattern: "*.m"
        fileTags: ["objc"]
    }

    FileTagger {
        pattern: "*.mm"
        fileTags: ["objcpp"]
    }

    FileTagger {
        pattern: "*.h"
        fileTags: ["hpp"]
    }

    FileTagger {
        pattern: "*.H"
        fileTags: ["hpp"]
    }

    FileTagger {
        pattern: "*.hpp"
        fileTags: ["hpp"]
    }

    FileTagger {
        pattern: "*.hxx"
        fileTags: ["hpp"]
    }

    FileTagger {
        pattern: "*.h++"
        fileTags: ["hpp"]
    }
}
