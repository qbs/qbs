import qbs

Module {
    Depends { name: "lower" }
    lower.listProp: ["higher"]
}
