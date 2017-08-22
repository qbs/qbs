import qbs

CppApplication {
    name: "app"

    files: ["main.cpp"]

    Probe {
        id: compilerProbe
        property stringList toolchain: qbs.toolchain
        property string compilerVersion: cpp.compilerVersion

        configure: {
            var isNewerMsvc;
            var isOlderMsvc;
            var isGcc;
            if (toolchain.contains("msvc")) {
                if (compilerVersion >= "18.00.30723")
                    isNewerMsvc = true;
                else
                    isOlderMsvc = true;
            } else {
                isGcc = true;
            }
            console.info("is newer MSVC: " + isNewerMsvc);
            console.info("is older MSVC: " + isOlderMsvc);
            console.info("is GCC: " + isGcc);
        }
    }
}
