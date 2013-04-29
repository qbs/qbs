import qbs 1.0

Project {
    Application {
        name: "myapp"
        Depends { name: "mylib" }
    }
    StaticLibrary {
        name: "mylib"
        Depends { name: "dummy" }
        dummy.defines: ["BUILD_MYLIB"]
        Export {
            Depends { name: "dummy" }
            dummy.defines: ["USE_MYLIB"]
        }
    }

    Application {
        name: "A"
        Depends { name: "B" }
    }
    StaticLibrary {
        name: "B"
        Export {
            Depends { name: "C" }
        }
    }
    StaticLibrary {
        name: "C"
        Export {
            Depends { name: "D" }
        }
    }
    StaticLibrary {
        name: "D"
    }
}
