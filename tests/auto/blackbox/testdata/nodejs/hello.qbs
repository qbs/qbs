import qbs.Host

NodeJSApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch ("
                         + qbs.targetPlatform + "/" + qbs.architecture + " vs "
                         + Host.platform() + "/" + Host.architecture() + ")");
        return result;
    }
    nodejs.applicationFile: "hello.js"
    name: "hello"
}
