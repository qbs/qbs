import qbs

Module {
    Depends { name: "gmod2" }
    property stringList listProp1: ["prototype", stringProp]
    property stringList listProp2: ["prototype"]
    property string stringProp: "prototype"
    property int depProp: gmod2.prop
    property int p0: p1 + p2
    property int p1: 0
    property int p2: 0
}
