import qbs.base 1.0

Product {
    type: ['mac' ? "applicationbundle" : "application", "installed_content"]
    Depends { id: qtcore; name: "Qt.core" }
    Depends { name: "qt.declarative" }
    Depends { name: "cpp" }
    property string appViewerPath: localPath + "/qmlapplicationviewer"
    cpp.includePaths: [appViewerPath]

    Group {
        condition: qtcore.versionMajor === 4
        files: [
            appViewerPath + "/qmlapplicationviewer_qt4.h",
            appViewerPath + "/qmlapplicationviewer_qt4.cpp"
        ]
    }

    Group {
        condition: qtcore.versionMajor === 5
        files: [
            appViewerPath + "/qmlapplicationviewer_qt5.h",
            appViewerPath + "/qmlapplicationviewer_qt5.cpp"
        ]
    }

    FileTagger {
        pattern: "*.qml"
        fileTags: ["install"]
    }
}

