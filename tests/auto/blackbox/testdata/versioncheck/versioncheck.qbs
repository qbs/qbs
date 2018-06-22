Product {
    property string requestedMinVersion
    property string requestedMaxVersion
    Depends { name: "higher" }
    Depends {
        name: "lower"
        versionAtLeast: requestedMinVersion
        versionBelow: requestedMaxVersion
    }
}
