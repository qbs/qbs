QtApplication {
    name: "app"
    cpp.cxxLanguageVersion: "c++11"
    files: "main.cpp"
    Group {
        name: "src1"
        Qt.core.generatedHeadersDir: product.buildDirectory + "/qt.headers_src1"
        prefix: name + '/'
        files: [
            "someclass.cpp",
            "someclass.h",
            "somefile.cpp",
        ]
    }
    Group {
        name: "src2"
        prefix: name + '/'
        Qt.core.generatedHeadersDir: product.buildDirectory + "/qt.headers_src2"
        files: [
            "someclass.cpp",
            "someclass.h",
            "somefile.cpp",
        ]
    }
}
