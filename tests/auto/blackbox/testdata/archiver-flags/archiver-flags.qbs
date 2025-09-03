StaticLibrary {
    Depends { name: "cpp" }
    files: "lib.cpp"

    Properties {
        condition: toolchainProbe.found
        cpp.archiverFlags: ["/WX:NO"]
    }

    Probe {
        id: toolchainProbe
        condition: qbs.toolchain.includes("msvc")
        configure: {
            console.info("toolchain is MSVC");
            found = true;
        }
    }
}
