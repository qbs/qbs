Module {
    Depends { name: "dummyqt.core" }
    property string guiProperty: "guiProperty"
    property string someString: "ene mene muh"

    Depends { name: "dummy" }
    dummy.defines: ["QT_GUI"]
    dummy.someString: someString
    dummy.zort: dummyqt.core.zort
}
