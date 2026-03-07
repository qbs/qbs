import qbs.Host

Project {
    CppApplication {
        condition: {
            var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
            if (!result)
                console.info("target platform/arch differ from host platform/arch ("
                             + qbs.targetPlatform + "/" + qbs.architecture + " vs "
                             + Host.platform() + "/" + Host.architecture() + ")");
            return result;
        }
        name: "testapp"
        Depends { name: "testlib"}
        files: ["testapp.cpp"]
    }
    DynamicLibrary {
        name: "testlib"
        Depends { name: "cpp"}
        files: ["testlib.cpp", "testlib.def"]
    }
}
