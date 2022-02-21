StaticLibrary {
    name: "quickjs"

    Depends { name: "qbsbuildconfig" }
    Depends { name: "cpp" }

    cpp.cLanguageVersion: qbs.toolchain.contains("msvc") ? "c99" : "gnu99"
    cpp.defines: ['CONFIG_VERSION="2021-03-27"'].concat(qbsbuildconfig.dumpJsLeaks
                                                        ? "DUMP_LEAKS" : [])
    cpp.warningLevel: "none"

    files: [
        "cutils.c",
        "cutils.h",
        "libregexp-opcode.h",
        "libregexp.c",
        "libregexp.h",
        "libunicode-table.h",
        "libunicode.c",
        "libunicode.h",
        "list.h",
        "quickjs-atom.h",
        "quickjs-opcode.h",
        "quickjs.c",
        "quickjs.diff",
        "quickjs.h",
    ]

    Export {
        Depends { name: "cpp" }
        cpp.includePaths: [exportingProduct.sourceDirectory]
    }
}
