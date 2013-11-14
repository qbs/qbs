Product {
    Depends { name: "nsis"; condition: qbs.targetOS.contains("windows") }
    type: ["nsissetup"]
}
