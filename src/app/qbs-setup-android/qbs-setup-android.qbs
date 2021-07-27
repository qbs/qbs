QbsApp {
    name: "qbs-setup-android"
    files: [
        "android-setup.cpp",
        "android-setup.h",
        "commandlineparser.cpp",
        "commandlineparser.h",
        "main.cpp",
    ]
    Group {
        name: "MinGW specific files"
        condition: qbs.toolchain.contains("mingw")
        files: "qbs-setup-android.rc"
        Group {
            name: "qbs-setup-android manifest"
            files: "qbs-setup-android.exe.manifest"
            fileTags: []  // the manifest is referenced by the rc file
        }
    }
}
