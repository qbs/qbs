import qbs 1.0

Product {
    property bool defineBase: true
    Depends { name: "dummy" }
    Properties {
        condition: defineBase
        dummy.defines: ["BASE"]
    }
    dummy.defines: ["SOMETHING"]
}
