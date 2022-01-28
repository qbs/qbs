import qbs.FileInfo
import qbs.Host

Project {
    property bool enabled: {
        var result = qbs.targetPlatform === Host.platform();
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result;
    }
    property string name: 'configure'
    qbsSearchPaths: '.'
    Product {
        type: 'application'
        consoleApplication: true
        name: project.name
        files: 'main.cpp'
        Depends { name: 'cpp' }
        Depends { name: 'definition' }
    }
}
