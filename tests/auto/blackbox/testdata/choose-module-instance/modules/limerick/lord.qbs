Module {
    condition: qbs.targetOS.containsAny(["Deep Purple", "Whitesnake"])
    version: "3"
    Depends { name: "texttemplate" }
    texttemplate.dict: ({DID: "chewed", THING: "cord", IDOL: "lord"})
}
