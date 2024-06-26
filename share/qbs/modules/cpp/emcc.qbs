import qbs
import qbs.ModUtils

GenericGCC
{
    condition: qbs.toolchain && qbs.toolchain.includes("emscripten")
    priority:  100
    qbs.targetPlatform: "wasm-emscripten"
    cCompilerName: "emcc"
    cxxCompilerName: "em++"
    archiverName: "emar"
    dynamicLibrarySuffix: ".so"
    dynamicLibraryPrefix: "lib"
    executableSuffix: ".js"
    rpathOrigin: "irrelevant"//to pass tests
    imageFormat: "wasm"

    Depends { name: "emsdk" }
    emsdk.configuredInstallPath: toolchainInstallPath
    toolchainInstallPath: emsdk.detectedInstallPath ? emsdk.detectedInstallPath : undefined
    probeEnv: emsdk.environment

    setupBuildEnvironment: {
        for (var key in product.cpp.probeEnv) {
            var v = new ModUtils.EnvironmentVariable(key);
            v.value = product.cpp.probeEnv[key];
            v.set();
        }
    }
}
