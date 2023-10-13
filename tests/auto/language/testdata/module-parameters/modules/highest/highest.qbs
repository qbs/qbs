Module {
    Depends {
        name: "higher"
        condition: project.overrideFromModule
        lower.param: "fromModuleDepends"
    }
}
