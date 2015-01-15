import qbs

Module {
    Depends { name: "lowerlevel" }
    lowerlevel.prop: "value in higherlevel"
}
