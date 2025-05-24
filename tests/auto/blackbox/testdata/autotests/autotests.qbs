import qbs.Host

Project {
    references: ["test1", "test2", "test3"]
    AutotestRunner {
        condition: {
            var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
            if (!result)
                console.info("target platform/arch differ from host platform/arch");
            return result;
        }
        Depends {
            name: "cpp" // Make sure build environment is set up properly.
            condition: Host.os().includes("windows") && qbs.toolchain.includes("gcc")
        }
    }
}
