import qbs.FileInfo
import qbs.Host

Project {
    property bool enabled: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch ("
                         + qbs.targetPlatform + "/" + qbs.architecture + " vs "
                         + Host.platform() + "/" + Host.architecture() + ")");
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
