import qbs 1.0

Project {

    Product {
        type: ["dynamiclibrary", "installed_content"]
        name: "bar"

        Depends { name: "cpp" }
        Depends { name: "qt"; submodules: ['core'] }

        files: [
            "src/bar.cpp"
        ]

        Group {
            condition: Qt.core.versionMajor === 4
            overrideTags: false
            qbs.installDir: "include/bar"
            fileTags: ["install"]
            files: ["src/bar_qt4.h"]
        }
        Group {
            condition: Qt.core.versionMajor === 5
            overrideTags: false
            qbs.installDir: "include/bar"
            fileTags: ["install"]
            files: ["src/bar_qt5.h"]
        }
    }
}


