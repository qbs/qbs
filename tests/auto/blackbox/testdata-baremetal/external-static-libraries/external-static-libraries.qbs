import "../BareMetalApplication.qbs" as BareMetalApplication
import "../BareMetalStaticLibrary.qbs" as BareMetalStaticLibrary

Project {
    property string outputLibrariesDirectory: sourceDirectory + "/libs"
    BareMetalStaticLibrary {
        name: "lib-a"
        destinationDirectory: project.outputLibrariesDirectory
        Depends { name: "cpp" }
        Properties {
            condition: qbs.targetOS.includes("darwin")
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
            condition: qbs.targetOS.includes("darwin")
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
