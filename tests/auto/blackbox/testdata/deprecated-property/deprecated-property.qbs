import qbs // FIXME: Don't remove this import because then the test fails!

Product {
    Depends { name: "themodule" }
    themodule.newProp: true
    themodule.expiringProp: false
    themodule.veryOldProp: false
}
