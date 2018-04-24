import "dummy_base.qbs" as DummyBase

DummyBase {
    condition: true

    additionalProductTypes: ["tag2"]

    property bool falseProperty: false
    property stringList defines
    property stringList cFlags
    property stringList cxxFlags
    property stringList rpaths: ["$ORIGIN"]
    property string someString
    property string productName: product.name
    property string upperCaseProductName: productName.toUpperCase()
    property string zort: "zort in dummy"
    property pathList includePaths
    property path somePath
    property stringList listProp: product.type.contains("blubb") ? ["123"] : ["456"]

    property bool controllingProp: false
    property stringList listProp2: controllingProp
                                   ? ["DEFAULT_STUFF", "EXTRA_STUFF"] : ["DEFAULT_STUFF"]
    property varList varListProp: [{a: controllingProp, b: someString}]
}
