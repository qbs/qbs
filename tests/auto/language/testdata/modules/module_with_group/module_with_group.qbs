Module {
    Depends { name: "dummy" }
    property string moduleDefine: "module_define"
    Group {
        name: "module_group"
        Properties {
            dummy.defines: [moduleDefine.toUpperCase(), name.toUpperCase()]
        }
    }
}
