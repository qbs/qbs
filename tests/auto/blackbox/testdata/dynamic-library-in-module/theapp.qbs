import qbs.Host

Project {
    CppApplication {
        condition: {
            var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
            if (!result)
                console.info("target platform/arch differ from host platform/arch");
            return result;
        }
        name: "theapp"
        cpp.minimumMacosVersion: "10.7" // For -rpath
        Depends { name: "theotherlib" }
        Depends { name: "thethirdlib" }
        Depends { name: "thefourthlib" }
        Depends { name: "staticlib" }
        files: "main.cpp"
        Group {
            fileTagsFilter: "dynamiclibrary"
            qbs.install: true
            qbs.installSourceBase: buildDirectory
        }
    }
    Dll {
        name: "thefourthlib"
        Depends { name: "thelib" }
        files: "lib4.cpp"
        Export {
            Depends { name: "cpp" }
            cpp.rpaths: [qbs.installRoot]
        }
    }
    StaticLibrary {
        name: "staticlib"
        Depends { name: "cpp" }
        Depends { name: "theotherlib" }
        files: "lib5.cpp"
    }
}
