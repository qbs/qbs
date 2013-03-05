import qbs 1.0
import "../scannerplugin.qbs" as ScannerPlugin

ScannerPlugin {
    cpp.defines: ["CPLUSPLUS_NO_PARSER"]
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

