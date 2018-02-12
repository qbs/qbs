import qbs

StaticLibrary {
    name: "qbsjson"
    Depends { name: "cpp" }
    cpp.cxxLanguageVersion: "c++14"
    cpp.minimumMacosVersion: "10.7"
    files: [
        "json.cpp",
        "json.h",
    ]
    Export {
        Depends { name: "cpp" }
        cpp.includePaths: [product.sourceDirectory]
    }
}
