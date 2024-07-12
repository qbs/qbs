Product {
    name: "in-group"
    property bool featureEnabled: true
    Depends { name: "dummy" }
    Depends { name: "module_with_group" }
    dummy.defines: ["BASEDEF"]
    Group {
        condition: featureEnabled
        name: "the group"
        files: ["dummy.txt" ]
        Properties {
            condition: name === "the group"
            dummy.defines: outer.concat("FEATURE_ENABLED", name.toUpperCase().replace(' ', '_'))
        }
        dummy.defines: "GROUP_ONLY"
    }
}
