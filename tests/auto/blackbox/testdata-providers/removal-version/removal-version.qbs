Project {
    qbsModuleProviders: "provider_a"
    name: "project"
    Project {
        name: "innerProject"
        Product {
            name: "p1"
            Depends { name: "qbsmetatestmodule" }
        }
    }

}
