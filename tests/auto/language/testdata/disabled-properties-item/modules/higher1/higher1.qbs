Module {
    Depends { name: "lower" }
    property bool setProp
    property string value: "fromHigher1"
    Properties {
        condition: setProp
        lower.prop: value
        lower.listProp: "WITH_HIGHER1_PROP"
    }
    Properties {
        condition: lower.n > 5
        lower.listProp: "N_GREATER_5"
    }
    Properties {
        condition: lower.n < 10
        lower.listProp: "N_LESS_10"
    }
    Properties {
        condition: lower.n < 20
        lower.listProp: "N_LESS_20"
    }
}
