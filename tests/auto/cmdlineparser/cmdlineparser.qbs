import qbs

QbsAutotest {
    Depends { name: "qbsversion" }
    testName: "cmdlineparser"
    files: ["tst_cmdlineparser.cpp", "../../../src/app/qbs/qbstool.cpp"]
    // TODO: Use Utilities.cStringQuote
    cpp.defines: base.concat([
        'SRCDIR="' + path + '"',
        "QBS_VERSION=\"" + qbsversion.version + "\""
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
