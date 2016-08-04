import qbs 1.0

Project {
    name: "blackbox tests"

    QbsAutotest {
        testName: "blackbox"
        Depends { name: "qbs_app" }
        Depends { name: "qbs-setup-toolchains" }
        Group {
            name: "testdata"
            prefix: "testdata/"
            files: ["**/*"]
            fileTags: []
        }
        files: [
            "../shared.h",
            "tst_blackboxbase.cpp",
            "tst_blackboxbase.h",
            "tst_blackbox.cpp",
            "tst_blackbox.h",
        ]
        cpp.defines: base.concat(['SRCDIR="' + path + '"'])
    }

    QbsAutotest {
        testName: "blackbox-java"
        Depends { name: "qbs_app" }
        Depends { name: "qbs-setup-toolchains" }
        Group {
            name: "testdata"
            prefix: "testdata-java/"
            files: ["**/*"]
            fileTags: []
        }
        files: [
            "../shared.h",
            "tst_blackboxbase.cpp",
            "tst_blackboxbase.h",
            "tst_blackboxjava.cpp",
            "tst_blackboxjava.h",
        ]
        cpp.defines: base.concat(['SRCDIR="' + path + '"'])
    }
}
