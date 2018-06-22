import qbs.File

Module {
    Depends { name: "depmodule" }
    Properties {
        condition: File.exists("blubb")
        depmodule.prop: "blubb"
    }
}
