Module {
    condition: qbs.targetOS.containsAny(["Deep Purple", "Rainbow"])
    priority: 1     // Overrides the more general "lord.qbs" instance,
                    // which also matches on "Deep Purple".
    version: "2"
    Depends { name: "texttemplate" }
    texttemplate.dict: ({DID: "slipped", THING: "litchi", IDOL: "ritchie"})
}
