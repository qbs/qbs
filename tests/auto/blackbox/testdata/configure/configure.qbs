import qbs.FileInfo
import qbs.Host

Project {
    property bool enabled: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
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
