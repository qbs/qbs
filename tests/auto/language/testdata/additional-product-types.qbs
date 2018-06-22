Product {
    name: "p"
    type: ["tag1"]

    Depends { name: "dummy" }
    Depends { name: "dummy2" }

    property bool hasTag1: type.contains("tag1")
    property bool hasTag2: type.contains("tag2")
    property bool hasTag3: type.contains("tag3")
    property bool hasTag4: type.contains("tag4")
}
