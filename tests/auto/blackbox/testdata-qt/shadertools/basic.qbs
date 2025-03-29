import qbs

QtApplication {
    name: "shadertools-test"
    type: "application"

    Depends { name: "Qt.shadertools" }
    Qt.shadertools.useQt6Versions: true
    Qt.shadertools.optimized: true
    Qt.shadertools.debugInformation: true

    files: ["basic_shader.frag", "main.cpp"]
}