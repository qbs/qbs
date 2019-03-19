Project {
    DynamicLibrary {
        name: "the_lib"
        Depends { name: "cpp" }
        files: ["lib.cpp", "lib.h"]
        Export {
            Depends { name: "cpp" }
            cpp.includePaths: path
        }

        Probe {
            id: toolchainProbe
            property stringList toolchain: qbs.toolchain
            configure: {
                if (toolchain.contains("msvc") && !toolchain.contains("clang-cl"))
                    console.info("compiler is MSVC")
                else
                    console.info("compiler is not MSVC")
            }
        }
    }
    CppApplication {
        name: "the_app"
        property bool link
        Depends { name: "the_lib"; cpp.link: product.link }
        files: "main.cpp"
    }
}
