Product {
    property bool isBlubbOS: qbs.targetOS.contains("blubb-OS")
    qbs.profiles: isBlubbOS ? ["blubb-profile"] : [project.profile]
    qbs.architecture: "blubb-arch"
}
