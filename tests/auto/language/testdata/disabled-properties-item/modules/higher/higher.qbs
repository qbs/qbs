Module {
    Depends { name: "lower" }
    property bool setProp
    property string value: "fromHigher"
    Properties {
        condition: setProp
        lower.prop: value
    }
}
