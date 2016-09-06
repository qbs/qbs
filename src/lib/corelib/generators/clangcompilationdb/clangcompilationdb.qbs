import qbs

QbsLibrary {
    type: ["staticlibrary"]
    name: "clangcompilationdbgenerator"
    install: false

    cpp.includePaths: base.concat([
        "../..",
    ])

    Depends { name: "cpp" }
    Depends { name: "Qt.core" }

    files: [
        "clangcompilationdbgenerator.cpp",
        "clangcompilationdbgenerator.h"
    ]
}
