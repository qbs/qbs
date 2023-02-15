Product {
    property bool isBlubbOS: qbs.targetOS.includes("blubb-OS")
    qbs.profiles: isBlubbOS ? ["blubb-profile"] : [project.profile]
    qbs.architecture: "blubb-arch"
}
