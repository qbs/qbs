Module {
    property bool setProp
    property string value: "fromLower"
    property string prop: "default"
    Properties {
        condition: setProp
        prop: outer + "_" + value
    }
}
