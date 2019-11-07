import "MyStaticLib.qbs" as MyStaticLib

Project {
    property bool removeDuplicates
    property string libDir: buildDirectory + "/lib"
    property bool dummy: {
        console.info("is bfd linker: "
                     + (qbs.toolchain.contains("gcc") && !qbs.hostOS.contains("macos")))
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
