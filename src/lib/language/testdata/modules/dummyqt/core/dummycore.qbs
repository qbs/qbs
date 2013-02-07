import qbs.base 1.0

Module {
    property int versionMajor: 5
    property int versionMinor: 0
    property int versionPatch: 0
    property string version: versionMajor.toString() + "." + versionMinor.toString() + "." + versionPatch.toString()
    property string coreProperty: "coreProperty"
}
