Product {
    name: "p"
    multiplexByQbsProperties: ["profiles"]
    qbs.profiles: ["theProfile"]
    Depends { name: "multiple_backends_via_own_property" }
    Profile {
        name: "theProfile"
        multiple_backends_via_own_property.type: "type1"
    }
}
