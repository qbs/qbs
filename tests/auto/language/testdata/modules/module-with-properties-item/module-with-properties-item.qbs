Module {
    property bool boolProperty: true
    property string stringProperty: "set in Module item"
    Properties {
        condition: boolProperty
        stringProperty: "overridden in Properties item"
    }
}
