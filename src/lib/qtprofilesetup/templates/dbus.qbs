import qbs 1.0
import qbs.FileInfo
import qbs.ModUtils
import "../QtModule.qbs" as QtModule
import "dbus.js" as DBus

QtModule {
    qtModuleName: "DBus"

    property string xml2cppName: "qdbusxml2cpp"
    property stringList xml2CppHeaderFlags: []
    property stringList xml2CppSourceFlags: []

    Rule {
        inputs: ["qt.dbus.adaptor"]

        Artifact {
            filePath: FileInfo.joinPaths(product.moduleProperty("Qt.core", "generatedHeadersDir"),
                                         DBus.outputFileName(input, "_adaptor.h"))
            fileTags: ["hpp"]
        }
        Artifact {
            filePath: DBus.outputFileName(input, "_adaptor.cpp")
            fileTags: ["cpp"]
        }

        prepare: {
            return DBus.createCommands(product, input, outputs, "-a");
        }
    }

    Rule {
        inputs: ["qt.dbus.interface"]

        Artifact {
            filePath: FileInfo.joinPaths(product.moduleProperty("Qt.core", "generatedHeadersDir"),
                                         DBus.outputFileName(input, "_interface.h"))
            fileTags: ["hpp"]
        }
        Artifact {
            filePath: DBus.outputFileName(input, "_interface.cpp")
            fileTags: ["cpp"]
        }

        prepare: {
            return DBus.createCommands(product, input, outputs, "-p");
        }
    }

    architecture: @arch@
    staticLibsDebug: @staticLibsDebug@
    staticLibsRelease: @staticLibsRelease@
    dynamicLibsDebug: @dynamicLibsDebug@
    dynamicLibsRelease: @dynamicLibsRelease@
    linkerFlagsDebug: @linkerFlagsDebug@
    linkerFlagsRelease: @linkerFlagsRelease@
    frameworksDebug: @frameworksDebug@
    frameworksRelease: @frameworksRelease@
    frameworkPathsDebug: @frameworkPathsDebug@
    frameworkPathsRelease: @frameworkPathsRelease@
    libNameForLinkerDebug: @libNameForLinkerDebug@
    libNameForLinkerRelease: @libNameForLinkerRelease@
    libFilePathDebug: @libFilePathDebug@
    libFilePathRelease: @libFilePathRelease@

    cpp.defines: @defines@
    cpp.includePaths: @includes@
    cpp.libraryPaths: @libraryPaths@

    @special_properties@
}

