// base for Cpp modules

Module {
    condition: false
    additionalProductFileTags: ["hpp"]  // to include all rules that generate hpp files

    property string warningLevel : 'all' // 'none', 'all'
    property bool treatWarningsAsErrors : false
    property string architecture: qbs.architecture
    property string optimization: qbs.optimization
    property bool debugInformation: qbs.debugInformation
    property string precompiledHeader
    property paths precompiledHeaderDir: [product.buildDirectory]
    property var defines
    property var platformDefines
    property var compilerDefines
    PropertyOptions {
        name: "compilerDefines"
        description: "preprocessor macros that are defined when using this particular compiler"
    }

    property paths includePaths
    property paths libraryPaths
    property paths frameworkPaths
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
