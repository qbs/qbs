Project {
    property bool dummy: {
        if (qbs.toolchain.includes("emscripten")) {
            console.info("is emscripten");
        } else if (qbs.targetOS.includes("windows")) {
            console.info("is windows");
        } else if (qbs.targetOS.includes("darwin")) {
            console.info("is darwin");
        } else if (qbs.targetOS.includes("unix")){
            console.info("is unix");
        }
    }

    CppLibrary {
        name: "testlib"
        publicHeaders: ["TestLib.h"]
        privateHeaders: ["TestLibPrivate.h"]
        files: ["TestLib.cpp"]
        qbs.installPrefix: ""

        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
    }
}
