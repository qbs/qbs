import qbs 1.0

Project {

    Product {
        type: ["dynamiclibrary"]
        name: "bar"

        Depends { name: "cpp" }
        Depends { name: "Qt"; submodules: ['core'] }

        files: [
            "src/bar.cpp"
        ]

        Group {
            condition: Qt.core.versionMajor === 4
            qbs.install: true
            qbs.installDir: "include/bar"
            files: ["src/bar_qt4.h"]
        }
        Group {
            condition: Qt.core.versionMajor === 5
            qbs.install: true
            qbs.installDir: "include/bar"
            files: ["src/bar_qt5.h"]
        }
    }
}


