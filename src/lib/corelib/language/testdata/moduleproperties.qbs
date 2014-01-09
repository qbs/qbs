import qbs 1.0

Project {
    Product {
        name: "merge_lists"
        Depends { name: "dummyqt"; submodules: ["gui", "network"] }
        Depends { name: "dummy" }
        dummy.defines: ["THE_PRODUCT"]
    }
    Product {
        name: "merge_lists_and_values"
        Depends { name: "dummyqt"; submodules: ["network", "gui"] }
        Depends { name: "dummy" }
        dummy.defines: "THE_PRODUCT"
    }
    Product {
        name: "merge_lists_with_duplicates"
        Depends { name: "dummy" }
        dummy.cxxFlags: ["-foo", "BAR", "-foo", "BAZ"]
    }
}
