Product {
    name: "p"
    type: ["tag1"]

    Depends { name: "dummy" }
    Depends { name: "dummy2" }

    property bool hasTag1: type.includes("tag1")
    property bool hasTag2: type.includes("tag2")
    property bool hasTag3: type.includes("tag3")
    property bool hasTag4: type.includes("tag4")
}
