import qbs.Probes

Product {
    Depends { name: "cpp" }

    Probes.BinaryProbe {
        id: cmakeProbe
        names: "cmake"
    }

    Probes.BinaryProbe {
        id: ninjaProbe
        names: ["ninja"]
    }

    property bool test: {
        var data = {
            cmakeFound: cmakeProbe.found,
            cmakeFilePath: cmakeProbe.filePath,
            crossCompiling: qbs.targetPlatform !== qbs.hostPlatform,
            installPrefix: qbs.installPrefix
        };
        data.buildEnv = {}
        Object.assign(data.buildEnv, cpp.buildEnv);  // deep copy buildEnv from a probe
        if (qbs.toolchain.includes("gcc")) {
            data.buildEnv["CC"] = cpp.cCompilerName;
            data.buildEnv["CXX"] = cpp.cxxCompilerName;
        } else {
            data.buildEnv["CC"] = cpp.compilerName;
            data.buildEnv["CXX"] = cpp.compilerName;
        }

        if (ninjaProbe.found) {
            data.generator = "Ninja";
        } else {
            if (qbs.toolchain.includes("msvc")) {
                data.generator = "NMake Makefiles"
            } else if (qbs.toolchain.includes("mingw")) {
                data.generator = "MinGW Makefiles";
            } else  if (qbs.toolchain.includes("gcc")) {
                data.generator = "Unix Makefiles";
            }
        }
        console.info("---" + JSON.stringify(data) + "---");
    }
}
