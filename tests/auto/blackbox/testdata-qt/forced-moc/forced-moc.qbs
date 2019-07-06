QtApplication {
    condition: {
        var result = qbs.targetPlatform === qbs.hostPlatform;
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result;
    }
    files: "main.cpp"
    Group {
        name: "QObject service provider"
        files: "createqtclass.h"
        fileTags: ["hpp", "unmocable"]
    }
    Group {
        name: "QObject service user"
        files: "myqtclass.h"
        fileTags: ["hpp", "mocable"]
    }
}
