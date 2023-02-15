QtApplication {
    name: "qmlapp"
    Depends { name: "Qt.quick" }
    Depends { name: "Qt.android_support" }
    Properties {
        condition: qbs.targetOS.includes("android")
        Qt.android_support.extraPrefixDirs: path
    }
    Android.sdk.packageName: "my.qmlapp"
    Android.sdk.apkBaseName: name
    Qt.android_support.qmlImportPaths: path
    files: [
        "main.cpp",
        "qml.qrc",
    ]
}
