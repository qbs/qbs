import qbs

QtApplication {
    name: "shadertools-test"
    type: "application"

    Depends { name: "Qt.shadertools" }
    Qt.shadertools.multiView: true

    files: ["basic_shader.frag", "main.cpp"]
}