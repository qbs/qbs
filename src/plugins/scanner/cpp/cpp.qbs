import qbs 1.0
import "../../qbsplugin.qbs" as QbsPlugin

QbsPlugin {
    cpp.defines: base.concat(["CPLUSPLUS_NO_PARSER"])
    name: "qbs_cpp_scanner"
    files: [
        "../scanner.h",
        "CPlusPlusForwardDeclarations.h",
        "Lexer.cpp",
        "Lexer.h",
        "Token.cpp",
        "Token.h",
        "cpp_global.h",
        "cppscanner.cpp"
    ]
}

