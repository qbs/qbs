// $ g++ 'i like spaces.cpp' '-DSPACES="!have \\fun\x5c!\n"' '-DSPICES=%T% # && $$ 1>&2 '\''\n'\''\n' '-DSLICES=(42>24)' && ./a.out
// SPACES=!have \fun\!
// SPICES=%T% # && $$ 1>&2 '\n'
// SLICES=1

Project {
Application {
    Probe {
        id: dummy
        property bool isMingw: qbs.toolchain.contains("mingw")
        property bool isMsvc: qbs.toolchain.contains("msvc")
        property var buildEnv: cpp.buildEnv
        configure: {
            if (!buildEnv)
                return;
            if (isMsvc) {
                console.info("is msvc");
                console.info("INCLUDE=" + buildEnv["INCLUDE"]);
                console.info("LIB=" + buildEnv["LIB"]);
            } else if (isMingw) {
                console.info("is mingw");
                console.info("PATH=" + buildEnv["PATH"]);
            }
        }
    }

    targetName: "i like spaces"

    Depends {
        name: "cpp"
    }

    cpp.defines: base.concat([
        "SPACES=\"!have \\\\fun\\x5c!\\n\"",
        "SPICES=%T% # && $$ 1>&2 '\\n'\\n",
        "SLICES=(42>24)"
    ]);

    files: ["i like spaces.cpp"]
}
}
