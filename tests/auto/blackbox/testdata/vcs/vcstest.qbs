import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform();
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result;
    }
    Depends { name: "vcs" }
    vcs.headerFileName: "my-repo-state.h"
    files: ["main.cpp"]
}
