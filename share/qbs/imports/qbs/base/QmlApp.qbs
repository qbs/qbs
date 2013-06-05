import qbs 1.0

Product {
    type: qbs.targetOS.contains("darwin") ? "applicationbundle" : "application"
    Depends { name: "Qt"; submodules: ["core", "declarative"] }
    Depends { name: "cpp" }
    property string appViewerPath: localPath + "/qmlapplicationviewer"
    cpp.includePaths: [appViewerPath]

    Group {
        condition: Qt.core.versionMajor === 4
        files: [
            appViewerPath + "/qmlapplicationviewer_qt4.h",
            appViewerPath + "/qmlapplicationviewer_qt4.cpp"
        ]
    }

    Group {
        condition: Qt.core.versionMajor === 5
        files: [
            appViewerPath + "/qmlapplicationviewer_qt5.h",
            appViewerPath + "/qmlapplicationviewer_qt5.cpp"
        ]
    }
}

