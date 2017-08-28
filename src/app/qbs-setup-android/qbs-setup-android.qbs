import qbs

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
        files: ["qbs-setup-android.exe.manifest", "qbs-setup-android.rc"]
    }
}
