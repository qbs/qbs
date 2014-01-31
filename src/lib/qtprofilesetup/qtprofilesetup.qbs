import qbs
import "../Library.qbs" as QbsLibrary

QbsLibrary {
    name: "qbsqtprofilesetup"
    Depends { name: "qbscore" }

    Group {
        name: "Public API headers"
        files: [
            "qtprofilesetup.h",
            "use_installed_qtprofilesetup.pri",
        ]
        qbs.install: project.installApiHeaders
        qbs.installDir: headerInstallPrefix
    }

    files: [
        "qtprofilesetup.cpp",
        "templates.qrc",
        "templates/*"
    ]

    Export {
        Depends { name: "qbscore" }
    }
}
