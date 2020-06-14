Product {
    Depends { name: "a" }
    // tests VariantValue
    property string prop
    PropertyOptions {
        name: "prop"
        description: "Some prop"
        allowedValues: "foo"
    }
    // tests JSValue
    property string prop2 // setter for otherProp
    property string otherProp: prop2
    PropertyOptions {
        name: "otherProp"
        description: "Some other prop"
        allowedValues: "foo"
    }
    name: "p"
}
