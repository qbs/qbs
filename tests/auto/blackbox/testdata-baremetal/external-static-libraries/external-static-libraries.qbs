import "../BareMetalApplication.qbs" as BareMetalApplication
import "../BareMetalStaticLibrary.qbs" as BareMetalStaticLibrary

Project {
    condition: {
        // The KEIL C51/C251/C166 toolchains support only a
        // full paths to the external libraries.
        if (qbs.toolchainType === "keil") {
            if (qbs.architecture === "mcs51"
                || qbs.architecture === "mcs251"
                || qbs.architecture === "c166") {
                console.info("unsupported toolset: %%"
                    + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
                return false;
            }
        }
        return true;
    }
    property string outputLibrariesDirectory: sourceDirectory + "/libs"
    BareMetalStaticLibrary {
        name: "lib-a"
        destinationDirectory: project.outputLibrariesDirectory
        Depends { name: "cpp" }
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        files: ["lib-a.c"]
    }
    BareMetalStaticLibrary {
        name: "lib-b"
        destinationDirectory: project.outputLibrariesDirectory
        Depends { name: "cpp" }
        Depends { name: "lib-a" }
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        files: ["lib-b.c"]
    }
    BareMetalApplication {
        Depends { name: "lib-a"; cpp.link: false }
        Depends { name: "lib-b"; cpp.link: false }
        files: ["main.c"]
        cpp.libraryPaths: [project.outputLibrariesDirectory]
        cpp.staticLibraries: ["lib-b", "lib-a"]
    }
}
