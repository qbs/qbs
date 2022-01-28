import "MyStaticLib.qbs" as MyStaticLib
import qbs.Host

Project {
    property bool removeDuplicates
    property string libDir: buildDirectory + "/lib"
    property bool dummy: {
        // most BSD systems (including macOS) use LLVM linker now
        console.info("is bfd linker: "
                     + (qbs.toolchain.contains("gcc") && !Host.os().contains("bsd")))
    }

    qbsSearchPaths: "."
    MyStaticLib { name: "requestor1" }
    MyStaticLib { name: "requestor2"  }
    MyStaticLib { name: "provider"; Group { files: "provider2.c" } }

    CppApplication {
        consoleApplication: true
        Depends { name: "requestor1"; cpp.link: false }
        Depends { name: "requestor2"; cpp.link: false }
        Depends { name: "provider"; cpp.link: false }
        cpp.libraryPaths: project.libDir
        cpp.removeDuplicateLibraries: project.removeDuplicates
        cpp.staticLibraries: ["requestor1", "requestor2", "provider", "requestor2"]
        files: "main.c"
    }
}
