import qbs

QtApplication {
    name: "view-count-test"
    type: "application"

    Depends { name: "Qt.shadertools" }
    Qt.shadertools.useQt6Versions: true
    Qt.shadertools.viewCount: 4

    files: ["basic_shader.frag", "main.cpp"]
}