Product {
    name: "p"

    Depends { name: "higher" }

    property bool setProp
    property string value: "fromProduct"
    Properties {
        condition: setProp
        lower.prop: value
    }
}
