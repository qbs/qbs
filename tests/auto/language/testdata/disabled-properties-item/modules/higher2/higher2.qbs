Module {
    Depends { name: "lower" }
    property bool setProp
    Properties {
        condition: setProp
        lower.prop: undefined
    }
}
