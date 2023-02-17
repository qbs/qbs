import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform();
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result;
    }
    qbsSearchPaths: ".."
    Depends { name: "bisonhelper" }
    Depends { name: "lex_yacc" }
    lex_yacc.outputTag: "cpp"
    lex_yacc.yaccFlags: ["-l"]
    cpp.includePaths: [".", ".."]
    cpp.cxxLanguageVersion: "c++11"
    cpp.minimumMacosVersion: "10.7"
    consoleApplication: true
    Probe {
        id: pathCheck
        property string theDir: {
            if (qbs.targetOS.includes("windows")) {
                if (qbs.toolchain.includes("mingw"))
                    return cpp.toolchainInstallPath;
                if (qbs.toolchain.includes("clang") && qbs.sysroot)
                    return qbs.sysroot + "/bin";
            }
        }
        configure: {
            if (theDir)
                console.info("add to PATH: " + theDir);
            found = true;
        }
    }

    files: [
        "lexer.l",
        "parser.y",
        "types.h",
    ]
}
