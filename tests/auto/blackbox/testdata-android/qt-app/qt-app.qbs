Project {
    QtGuiApplication {
        Depends { name: "Lib" }
        files: ["main.cpp", "MainWindow.cpp", "MainWindow.h" ]
        Group {
            condition: Qt.core.versionMajor == 5
            files: ["Test.java"]
        }
        Group {
            condition: Qt.core.versionMajor == 6
            files: ["TestQt6.java"]
        }

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
