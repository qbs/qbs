Project {
    Product {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        name: 'someapp'
        type: 'application'
        consoleApplication: true
        Depends { name: 'cpp' }
        files: [ "main.cpp", "narf.h", "narf.cpp" ]
    }
}

