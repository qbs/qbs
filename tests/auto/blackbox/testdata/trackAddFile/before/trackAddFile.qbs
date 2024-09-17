import qbs.Host

Product {
    condition: {
        var result = qbs.targetPlatform === Host.platform();
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result;
    }
    name: 'someapp'
    type: 'application'
    consoleApplication: true

    property bool dummy: { console.info("object suffix: " + cpp.objectSuffix); }

    Depends { name: 'cpp' }
    files: [ "main.cpp", "narf.h", "narf.cpp" ]
}

