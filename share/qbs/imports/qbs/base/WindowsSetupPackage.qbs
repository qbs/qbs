Product {
    Depends { name: "wix"; condition: qbs.hostOS.contains("windows") && qbs.targetOS.contains("windows") }
    type: ["wixsetup"]
}
