Module {
    Depends { name: "dummy" }
    property string moduleDefine: "module_define"
    property string group
    Group {
        name: "module_group"
        condition: group == name
        Properties {
            dummy.defines: [moduleDefine.toUpperCase(), name.toUpperCase()]
        }
    }
    Group {
        name: "module_group_alt"
        condition: group == name
        product.dummy.defines: [moduleDefine.toUpperCase(), name.toUpperCase()]
    }
}
