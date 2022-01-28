import qbs.Host

NodeJSApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform();
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result;
    }
    nodejs.applicationFile: "hello.js"
    name: "hello"
}
