import qbs.base 1.0

Project {
    Application {
        name: "myapp"
        Depends { name: "mylib" }
    }
    StaticLibrary {
        name: "mylib"
        Depends { name: "dummy" }
        dummy.defines: ["BUILD_MYLIB"]
        ProductModule {
            Depends { name: "dummy" }
            dummy.defines: ["USE_MYLIB"]
        }
    }
}
