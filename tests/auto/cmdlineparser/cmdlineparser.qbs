import qbs
import qbs.Utilities

QbsAutotest {
    Depends { name: "qbsversion" }
    testName: "cmdlineparser"
    files: ["tst_cmdlineparser.cpp", "../../../src/app/qbs/qbstool.cpp"]
    cpp.defines: base.concat([
        "SRCDIR=" + Utilities.cStringQuote(path),
        "QBS_VERSION=" +Utilities.cStringQuote(qbsversion.version)
    ])

    // TODO: Make parser a static library?
    Group {
        name: "parser"
        prefix: "../../../src/app/qbs/parser/"
        files: [
            "commandlineoption.cpp",
            "commandlineoption.h",
            "commandlineoptionpool.cpp",
            "commandlineoptionpool.h",
            "commandlineparser.cpp",
            "commandlineparser.h",
            "commandpool.cpp",
            "commandpool.h",
            "commandtype.h",
            "parsercommand.cpp",
            "parsercommand.h",
        ]
    }
}
