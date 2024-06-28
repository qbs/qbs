Product {
    name: "p"

    Depends { name: "higher1" }
    Depends { name: "higher2" }

    property bool setProp
    property string value: "fromProduct"
    Properties {
        condition: setProp
        lower.prop: value
    }
}
