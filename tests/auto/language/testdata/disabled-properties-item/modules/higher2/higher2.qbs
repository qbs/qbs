Module {
    Depends { name: "lower" }
    property bool setProp
    Properties {
        condition: setProp
        lower.prop: undefined
        lower.listProp: "WITH_HIGHER2_PROP"
    }
    Properties {
        condition: lower.n > 6
        lower.listProp: "N_GREATER_6"
    }
}
