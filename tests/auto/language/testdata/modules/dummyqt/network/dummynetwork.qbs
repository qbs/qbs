import qbs.base 1.0

Module {
    Depends { name: "dummyqt"; submodules: ["core"] }
    property string networkProperty: "networkProperty"
}
