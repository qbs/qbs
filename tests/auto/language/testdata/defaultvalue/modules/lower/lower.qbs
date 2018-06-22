Module {
    property string prop1
    property string prop2: prop1 === "blubb" ? "withBlubb" : "withoutBlubb"
    property stringList listProp: prop1 === "blubb" ? ["blubb", "other"] : ["other"]
}
