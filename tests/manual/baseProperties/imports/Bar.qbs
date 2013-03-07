import qbs 1.0

Product {
    Depends { name: "cpp" }
    cpp.defines: ["FROM_BAR"]
}
