Project {
    QtGuiApplication {
        Depends { name: "Lib" }
        files: ["main.cpp", "MainWindow.cpp", "MainWindow.h"]
        Android.sdk.packageName: "my.qtapp"
        Android.sdk.apkBaseName: name
        Depends { name: "Qt"; submodules: ["core", "widgets"] }
    }

    StaticLibrary {
        name: "Lib"
        Export {
            Depends {
                name: "Qt.android_support";
            }
        }
    }
}
