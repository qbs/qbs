import qbs.Host

Project {
    property bool withDefinitionFile: true

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
        files: {
            var list = ["testlib.cpp"];
            if (project.withDefinitionFile)
                list.push("testlib.def");
            return list;
        }
        Group {
            condition: !project.withDefinitionFile
            product.cpp.visibility: "hidden" // for Unix GGC
            product.cpp.linkerFlags: qbs.toolchain.includes("mingw") // for MinGW
                         ? ["--exclude-all-symbols"] : []
        }
    }
}
