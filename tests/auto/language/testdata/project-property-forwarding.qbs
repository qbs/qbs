Project {
    property string prop: "parent"
    Project {
        property string prop: parent.prop + "2"
        Project {
            property bool dummy: { console.info("prop: " + prop); }
        }
    }
}
