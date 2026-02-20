Project {
    CppApplication {
        qbs.installPrefix: ""
        property bool dummy: {
            console.info("supports time trace: " + cpp._supportsTimeTrace);
        }
        name: "enable-time-trace"
        files: ["main.cpp"]
        cpp.enableTimeTrace: true

        Group {
            fileTagsFilter: "time_trace"
            qbs.install: true
            qbs.installDir: "share/enable-time-trace"
        }
    }
}
