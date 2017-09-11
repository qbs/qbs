Module {
    condition: qbs.targetOS.containsAny(["Deep Purple", "Rainbow"])
    priority: 1     // Overrides the more general "lord.qbs" instance,
                    // which also matches on "Deep Purple".
    Depends { name: "texttemplate" }
    texttemplate.dict: ({DID: "slipped", THING: "litchi", IDOL: "ritchie"})
}
