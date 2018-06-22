Product {
    name: "in-group"
    property bool featureEnabled: true
    Depends { name: "dummy" }
    dummy.defines: ["BASEDEF"]
    Group {
        name: "the group"
        files: ["dummy.txt" ]
        Properties {
            condition: featureEnabled
            dummy.defines: outer.concat("FEATURE_ENABLED")
        }
    }
}
