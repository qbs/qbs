import qbs

QbsLibrary {
    name: "qbsqtprofilesetup"
    Depends { name: "qbscore" }

    Group {
        name: "Public API headers"
        files: [
            "qtenvironment.h",
            "qtprofilesetup.h",
            "use_installed_qtprofilesetup.pri",
        ]
        qbs.install: qbsbuildconfig.installApiHeaders
        qbs.installDir: headerInstallPrefix
    }

    files: [
        "qtprofilesetup.cpp",
        "qtmoduleinfo.cpp",
        "qtmoduleinfo.h",
        "templates.qrc",
        "templates/*"
    ]

    Export {
        Depends { name: "qbscore" }
    }
}
