import qbs

Product {
    property bool isBlubbOS: qbs.targetOS.contains("blubb-OS")
    profiles: isBlubbOS ? ["blubb-profile"] : [project.profile]
    qbs.architecture: "blubb-arch"
}
