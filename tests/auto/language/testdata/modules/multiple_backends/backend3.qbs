Module {
    condition: qbs.targetOS.includes("os2") && qbs.toolchain.includes("tc")
    priority: 1
    property string backend3Prop
}
