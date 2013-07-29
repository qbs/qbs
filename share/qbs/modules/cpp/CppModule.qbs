// base for Cpp modules

Module {
    condition: false
    property string warningLevel : 'all' // 'none', 'all'
    property bool treatWarningsAsErrors : false
    property string architecture: qbs.architecture
    property string optimization: qbs.optimization
    property bool debugInformation: qbs.debugInformation
    property pathList prefixHeaders
    property path precompiledHeader
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
        description: "The prefix for the internal install name (LC_ID_DYLIB) of an OS X dynamic\
                      library."
    }

    property pathList includePaths
    property pathList systemIncludePaths
    property pathList libraryPaths
    property pathList frameworkPaths
    property pathList systemFrameworkPaths
    property string compilerName
    property string compilerPath: compilerName
    property stringList compilerWrapper
    property string staticLibraryPrefix
    property string dynamicLibraryPrefix
    property string executablePrefix
    property string staticLibrarySuffix
    property string dynamicLibrarySuffix
    property string executableSuffix
    property stringList dynamicLibraries // list of names, will be linked with -lname
    property stringList staticLibraries // list of static library files
    property stringList frameworks // list of frameworks, will be linked with '-framework <name>'
    property stringList weakFrameworks // list of weakly-linked frameworks, will be linked with '-weak_framework <name>'
    property stringList rpaths

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

    property string visibility: 'default' // 'default', 'hidden', 'hiddenInlines'
    PropertyOptions {
        name: "visibility"
        description: "export symbols visibility level"
        allowedValues: ['default', 'hidden', 'hiddenInlines']
    }

    // Platform properties. Those are intended to be set by the toolchain setup
    // and are prepended to the corresponding user properties.
    property stringList platformCommonCompilerFlags
    property stringList platformCFlags
    property stringList platformCxxFlags
    property stringList platformObjcFlags
    property stringList platformObjcxxFlags
    property stringList platformLinkerFlags

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
        pattern: "*.cc"
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
