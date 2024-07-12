Module {
    property int versionMajor: 5
    property int versionMinor: 0
    property int versionPatch: 0
    property string version: versionMajor.toString() + "." + versionMinor.toString() + "." + versionPatch.toString()
    property string coreProperty: "coreProperty"
    property string coreVersion: version
    property string zort: "zort in dummyqt.core"

    Depends { name: "dummy" }
    dummy.defines: ["QT_CORE"]
    dummy.rpaths: ["/opt/qt/lib"]
    dummy.cFlags: [zort]
    dummy.varListProp: [{c: "qtcore"}]

    Properties {
        dummy.productName: product.name
    }
}
