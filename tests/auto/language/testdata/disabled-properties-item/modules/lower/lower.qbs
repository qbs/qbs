Module {
    property bool setProp
    property string value: "fromLower"
    property string prop: "default"
    property stringList listProp
    property int n: 0
    Group {
        condition: setProp
        module.prop: outer + "_" + value
        module.listProp: "WITH_LOWER_PROP"
    }
    Group {
        condition: n
        module.listProp: ["N_NON_ZERO"]
    }
}
