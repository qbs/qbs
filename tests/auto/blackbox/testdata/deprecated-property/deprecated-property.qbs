import qbs

Product {
    Depends { name: "themodule" }
    themodule.newProp: true
    themodule.oldProp: false
    themodule.veryOldProp: false
}
