Project {
    name: "plugin_type"

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

    Plugin {
        Depends { name: "bundle" }
        Depends { name: "cpp" }
        name: "myplugin"
        files: ["plugin-type.cpp"]
        qbs.installPrefix: ""
        installDir: config.install.pluginsDirectory // enforce install static plugins
        bundle.isBundle: false
    }
}


