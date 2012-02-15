import qbs.base 1.0

Product {
    type: ["application", "installed_content"]
    Depends { name: "qt.declarative" }
    Depends { name: "cpp" }
    property string appViewerPath: localPath + "/qmlapplicationviewer"
    cpp.includePaths: [appViewerPath]

    Group {
        files: [
            appViewerPath + "/qmlapplicationviewer.h",
            appViewerPath + "/qmlapplicationviewer.cpp"
        ]
    }

    FileTagger {
        pattern: "*.qml"
        fileTags: ["install"]
    }
}

