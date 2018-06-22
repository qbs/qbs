Module {
    Depends { name: "dummyqt"; submodules: ["core"] }
    property string networkProperty: "networkProperty"

    Depends { name: "dummy" }
    dummy.defines: ["QT_NETWORK"]
}
