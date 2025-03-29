import qbs.FileInfo
import qbs.Utilities

CppApplication {
    condition: Utilities.versionCompare(Qt.core.version, "6.5") >= 0 && !Qt.core.staticBuild

    Depends { name: "Qt.core" }
    Depends { name: "Qt.quick" }
    Depends { name: "Qt.qml" }
    Depends { name: "Qt.shadertools" }

    cpp.cxxLanguageVersion: "c++11"

    files: [
        "main.cpp",
        "resources.qrc",
        "shadereffects.qml",
    ]

    Group {
        name: "Shaders"
        files: [
            "content/shaders/genie.vert",
            "content/shaders/blur.frag",
            "content/shaders/colorize.frag",
            "content/shaders/outline.frag",
            "content/shaders/shadow.frag",
            "content/shaders/wobble.frag"
        ]
        Qt.core.resourcePrefix: "/qt/qml/shadereffects/content/shaders"
    }

    install: true
    qbs.installPrefix: ""
}
