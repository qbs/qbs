import qbs 1.0

Project {
    StaticLibrary {
        name : "testa"
        files : [ "testa.s" ]
        Depends { name: "cpp" }
        condition: qbs.toolchain.contains("gcc")
    }
    StaticLibrary {
        name : "testb"
        files : [ "testb.S" ]
        Depends { name: "cpp" }
        condition: qbs.toolchain.contains("gcc")
    }
    StaticLibrary {
        name : "testc"
        files : [ "testc.sx" ]
        Depends { name: "cpp" }
        condition: qbs.toolchain.contains("gcc")
    }
    StaticLibrary {
        name: "testd"
        Group {
            condition: product.condition
            files: ["testd_" + qbs.architecture + ".asm"]
        }
        Depends { name: "cpp" }
        condition: qbs.toolchain.contains("msvc")
                   && (qbs.architecture === "x86" || qbs.architecture === "x86_64")
    }
}

