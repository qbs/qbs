Project {
    property bool dummy: {
        const isWin = qbs.targetOS.includes("windows");
        const isDarwin = qbs.targetOS.includes("darwin");
        const isMac = qbs.targetOS.includes("macos");
        const toolGcc = qbs.toolchain.includes("gcc") || qbs.toolchain.includes("emscripten");
        console.info("is windows: " + (isWin ? "yes" : "no"));
        console.info("is darwin: " + (isDarwin ? "yes" : "no"));
        console.info("is macos: " + (isMac ? "yes" : "no"));
        console.info("is gcc: " + (toolGcc ? "yes" : "no"));
    }

    Library {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        name: "mylib"
        files: ["library-type.cpp"]
        qbs.installPrefix: ""
        config.install.staticLibraries: true
        bundle.isBundle: false
    }
}
