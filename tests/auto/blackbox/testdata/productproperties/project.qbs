import qbs 1.0

Project {
    property string blubbProp: "5"
    references: ["header.qbs", "app.qbs"]
}
