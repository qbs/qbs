Module {
    condition: qbs.targetOS.contains("os2") && qbs.toolchain.contains("tc")
    priority: 1
    property string backend3Prop
}
