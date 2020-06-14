Module {
    // tests VariantValue
    property stringList prop
    PropertyOptions {
        name: "prop"
        description: "Some prop"
        allowedValues: ["foo", "bar"]
    }
    // tests JSValue
    property stringList prop2 // setter for otherProp
    property stringList otherProp: prop2
    PropertyOptions {
        name: "otherProp"
        description: "Some other prop"
        allowedValues: ["foo", "bar"]
    }
}

