Product {
    Export {
        Depends { name: "dummy" }
        dummy.cxxFlags: ["-foo"]
        dummy.defines: ["ABC"]
    }
}
