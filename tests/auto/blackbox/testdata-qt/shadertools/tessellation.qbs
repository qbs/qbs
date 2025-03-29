import qbs

QtApplication {
    name: "shadertools-test"
    type: "application"

    Depends { name: "Qt.shadertools" }
    Qt.shadertools.glslVersions: ["410", "320es"]
    Qt.shadertools.tessellationVertexCount: 3
    Qt.shadertools.tessellationMode: "triangles"

    files: ["tessellation_shader.vert", "tessellation_shader.tesc", "main.cpp"]
}