CppApplication {
    name: "p"
    property string linkerVariant
    Probe {
        id: gccProbe
        property bool isGcc: qbs.toolchain.contains("gcc")
        configure: {
            console.info("is GCC: " + isGcc);
            if (isGcc)
                found = true;
        }
    }

    Properties {
        condition: gccProbe.found
        cpp.linkerVariant: linkerVariant
    }

    files: "main.cpp"
}
