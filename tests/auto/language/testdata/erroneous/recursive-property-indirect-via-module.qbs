Product {
    Depends { name: "recursion_helper" }
    property bool a: recursion_helper.a
}
