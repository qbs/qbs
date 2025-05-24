import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
        return result;
    }
    Depends { name: "vcs" }
    vcs.headerFileName: "my-repo-state.h"
    files: ["main.cpp"]
}
