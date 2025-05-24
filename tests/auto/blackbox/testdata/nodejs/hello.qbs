import qbs.Host

NodeJSApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
        return result;
    }
    nodejs.applicationFile: "hello.js"
    name: "hello"
}
