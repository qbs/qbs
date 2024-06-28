Module {
    Depends { name: "lower" }
    property bool setProp
    property string value: "fromHigher1"
    Properties {
        condition: setProp
        lower.prop: value
    }
}
