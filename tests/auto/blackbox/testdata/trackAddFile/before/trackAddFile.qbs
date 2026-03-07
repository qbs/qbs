import qbs.Host

Product {
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch ("
                         + qbs.targetPlatform + "/" + qbs.architecture + " vs "
                         + Host.platform() + "/" + Host.architecture() + ")");
        return result;
    }
    name: 'someapp'
    type: 'application'
    consoleApplication: true

    property bool dummy: { console.info("object suffix: " + cpp.objectSuffix); }

    Depends { name: 'cpp' }
    files: [ "main.cpp", "narf.h", "narf.cpp" ]
}

