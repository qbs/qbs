Module {
    Depends { name: "lower" }
    lower.prop2: lower.prop1 === "egon" ? "withEgon" : original
    lower.listProp: lower.prop1 === "egon" ? ["egon"] : original
}
