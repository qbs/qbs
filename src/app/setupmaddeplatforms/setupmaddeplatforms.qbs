import qbs 1.0
import "../apptemplate.qbs" as QbsApp

QbsApp {
    name: "qbs-setup-madde-platforms"
    files: [
        "../shared/qbssettings.h",
        "../shared/specialplatformssetup.cpp",
        "../shared/specialplatformssetup.h",
        "main.cpp"
    ]
}

