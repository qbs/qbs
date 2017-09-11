Module {
    condition: qbs.targetOS.containsAny(["Deep Purple", "Whitesnake"])
    Depends { name: "texttemplate" }
    texttemplate.dict: ({DID: "chewed", THING: "cord", IDOL: "lord"})
}
