Module {
    Depends { name: "lowerlevel" }
    lowerlevel.propDependency: "value in higherlevel"
}
