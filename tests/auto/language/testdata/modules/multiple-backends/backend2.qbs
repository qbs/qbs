import qbs

Module {
    condition: qbs.targetOS.contains("os2")
    property string prop: "backend 2"
}
