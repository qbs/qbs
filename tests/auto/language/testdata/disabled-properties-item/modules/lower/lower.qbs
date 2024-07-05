Module {
    property bool setProp
    property string value: "fromLower"
    property string prop: "default"
    property stringList listProp
    property int n: 0
    Properties {
        condition: setProp
        prop: outer + "_" + value
        listProp: "WITH_LOWER_PROP"
    }
    Properties {
        condition: n
        listProp: ["N_NON_ZERO"]
    }
}
