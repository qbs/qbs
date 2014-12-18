import qbs 1.0

Bar {
    type: "application"
    consoleApplication: true
    cpp.defines: base.concat(["FROM_FOO"])
}

