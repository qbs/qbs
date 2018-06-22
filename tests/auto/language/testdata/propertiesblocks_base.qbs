Product {
    property bool defineBase: true
    Depends { name: "dummy" }
    Properties {
        condition: defineBase
        dummy.defines: ["BASE"]
    }
    dummy.defines: ["SOMETHING"]
    property stringList myCFlags: ["BASE"]
    dummy.cFlags: myCFlags
}
