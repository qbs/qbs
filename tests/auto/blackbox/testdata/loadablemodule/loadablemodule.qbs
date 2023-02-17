import qbs.Host

Project {
    LoadableModule {
        Depends { name: "cpp" }
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
        name: "CoolPlugIn"
        files: ["exported.cpp", "exported.h"]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }

    CppApplication {
        condition: {
            var result = qbs.targetPlatform === Host.platform();
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        Depends { name: "cpp" }
        Depends { name: "CoolPlugIn"; cpp.link: false }
        consoleApplication: true
        name: "CoolApp"
        files: ["main.cpp"]

        cpp.cxxLanguageVersion: "c++11"
        cpp.dynamicLibraries: [qbs.targetOS.includes("windows") ? "kernel32" : "dl"]

        Properties {
            condition: qbs.targetOS.includes("unix") && !qbs.targetOS.includes("darwin")
            cpp.rpaths: [cpp.rpathOrigin]
        }

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }
}
