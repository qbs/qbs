import qbs 1.0

Module {
    Depends { name: "dummyqt.core" }
    property string guiProperty: "guiProperty"

    Depends { name: "dummy" }
    dummy.defines: ["QT_GUI"]
}
