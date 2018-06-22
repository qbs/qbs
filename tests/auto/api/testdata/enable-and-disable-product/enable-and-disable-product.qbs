CppApplication {
    property string prop: undefined // Influences source artifact properties and the product condition
    condition: prop
    cpp.visibility: prop
    files: "main.cpp"
}
