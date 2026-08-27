import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform()
                && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch ("
                            + qbs.targetPlatform + "/" + qbs.architecture + " vs "
                            + Host.platform() + "/" + Host.architecture() + ")");
        return result;
    }
    name: "app"
    consoleApplication: true
    cpp.cxxLanguageVersion: "c++11"
    cpp.minimumOsxVersion: "10.8" // For <chrono>
    Properties {
        condition: qbs.toolchain.includes("gcc")
        cpp.driverFlags: "-pthread"
    }
    files: "main.cpp"
}
