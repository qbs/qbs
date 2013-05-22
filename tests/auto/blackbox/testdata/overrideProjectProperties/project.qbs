import qbs 1.0

Project {
    property string nameSuffix: ""
    Product {
        type: "application"
        property string mainFile: ""
        name: "MyApp" + nameSuffix
        Depends { name: "cpp" }
        files: [mainFile]
    }
}
