Project {
    name: "My.Project"
    property bool x

    Product {
        name: "MyProduct"
        property bool x
    }

    Product {
        name: "MyOtherProduct"
        Depends { name: "cpp" }
    }
}
