import qbs

QtApplication {
    name: "shadertools-linking-test"
    type: "application"

    Depends { name: "Qt.shadertools" }

    files: ["shader.frag", "main.cpp"]
    Qt.shadertools.enableLinking: true
}