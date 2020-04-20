Project {
    CppApplication {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
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
