Module {
    Depends { name: "higher"; lower.param: "shouldNeverAppear" }
    validate: { throw "As the name indicates, this module is broken."; }
}
