Module {
    Depends { name: "dummy" }
    property string moduleDefine: "module_define"
    Group {
        name: "module_group"
        Properties {
            condition: true
            dummy.defines: [moduleDefine.toUpperCase(), name.toUpperCase()]
        }
    }
}
