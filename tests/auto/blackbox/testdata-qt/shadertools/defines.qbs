import qbs

QtApplication {
    name: "shadertools-defines-test"
    type: "application"

    Depends { name: "Qt.shadertools" }
    Qt.shadertools.useQt6Versions: true
    Qt.shadertools.defines: ["TEST_DEFINE_1=1", "TEST_DEFINE_2=1"]

    files: ["defines_shader.frag", "main.cpp"]
}