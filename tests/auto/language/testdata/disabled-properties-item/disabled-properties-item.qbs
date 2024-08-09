Product {
    name: "p"

    Depends { name: "higher1" }
    Depends { name: "higher2" }

    property bool setProp
    property string value: "fromProduct"
    property stringList productProp: "default"
    lower.n: 10
    Group {
        condition: setProp
        product.lower.prop: value
        product.lower.listProp: "WITH_PRODUCT_PROP"
        product.productProp: "condition1"
    }
    Group {
        condition: lower.n > 7
        product.lower.listProp: "N_GREATER_7"
        product.productProp: "condition2"
    }
}
