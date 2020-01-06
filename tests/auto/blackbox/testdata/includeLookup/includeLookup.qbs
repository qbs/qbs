import qbs.FileInfo

Project {
    property string name: 'includeLookup'
    qbsSearchPaths: '.'
    Product {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        type: 'application'
        consoleApplication: true
        name: project.name
        files: 'main.cpp'
        Depends { name: 'cpp' }
        Depends { name: 'definition' }
    }
}
