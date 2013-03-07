import qbs 1.0

Bar {
    type: "application"
    cpp.defines: base.concat(["FROM_FOO"])
}

