import qbs.Host

Project {
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
        return result;
    }
    DynamicLibrary {
        name: "testlib"
        Depends { name: "cpp"}
        files: ["testlib.cpp", "testlib.def"]
    }
    CppApplication {
        name: "testapp"
        Depends { name: "testlib"}
        files: ["testapp.cpp"]
    }
}
