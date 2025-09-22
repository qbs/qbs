Module {
    priority: -100

    property bool autotetect: false

    property string bin: "bin"
    property string etc: "etc"
    property string include: "include"
    property string lib: "lib"

    property string topLevelProjectName: {
        var p = project;
        while (p.parent !== undefined) {
            p = p.parent;
        }
        return p.name;
    }
    property string subdir: topLevelProjectName.toLowerCase();
    property string libexec: "libexec/" + subdir

    property string plugins: lib + "/" + subdir + "/plugins"

    property string share: "share/" + subdir

    property string applications: "Applications"
    property string frameworks: "Frameworks"
    property string library: "Library"
}
