Product {
    name: "p"

    Depends { name: "higher1" }
    Depends { name: "higher2" }

    property bool setProp
    property string value: "fromProduct"
    lower.n: 10
    Properties {
        condition: setProp
        lower.prop: value
        lower.listProp: "WITH_PRODUCT_PROP"
    }
    Properties {
        condition: lower.n > 7
        lower.listProp: "N_GREATER_7"
    }
}
