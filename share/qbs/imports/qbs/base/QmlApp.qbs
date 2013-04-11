import qbs 1.0

Product {
    type: qbs.targetPlatform.indexOf("darwin") !== -1 ? "applicationbundle" : "application"
    Depends { name: "qt"; submodules: ["core", "declarative"] }
    Depends { name: "cpp" }
    property string appViewerPath: localPath + "/qmlapplicationviewer"
    cpp.includePaths: [appViewerPath]

    Group {
        condition: qt.core.versionMajor === 4
        files: [
            appViewerPath + "/qmlapplicationviewer_qt4.h",
            appViewerPath + "/qmlapplicationviewer_qt4.cpp"
        ]
    }

    Group {
        condition: qt.core.versionMajor === 5
        files: [
            appViewerPath + "/qmlapplicationviewer_qt5.h",
            appViewerPath + "/qmlapplicationviewer_qt5.cpp"
        ]
    }
}

