Project {
    AppleApplicationDiskImage {
        Depends { name: "myapp" }
        Depends { name: "ib" }
        dmg.volumeName: "My Great App"
        dmg.iconSize: 128
        dmg.windowWidth: 640
        dmg.windowHeight: 280
        files: [
            "white.iconset",
        ]
    }

    CppApplication {
        name: "myapp"
        targetName: "My Great App"
        files: ["main.c"]

        install: true
    }

    AppleDiskImage {
        name: "hellodmg"
        targetName: "hellodmg-1.0"
                    + (qbs.architecture ? "-" + qbs.architecture : "")

        dmg.volumeName: "Hello DMG"

        files: [
            "hello.icns",
            "hello.tif"
        ]

        Group {
            files: ["en_US.lproj/eula.txt"]
            fileTags: ["dmg.input", "dmg.license.input"]
            dmg.iconX: 320
            dmg.iconY: 240
            dmg.licenseLocale: "en_US"
        }

        Group {
            files: ["*.lproj/**"]
            excludeFiles: ["en_US.lproj/eula.txt"]
        }
    }

    AppleDiskImage {
        name: "green"
        dmg.backgroundColor: "green"
    }

    AppleDiskImage {
        name: "german"
        dmg.defaultLicenseLocale: "de_DE"

        Group {
            files: ["*.lproj/**"]
        }
    }

    AppleDiskImage {
        name: "custom-buttons"

        Group {
            files: ["ru_RU.lproj/eula.txt"]
            dmg.licenseLocale: "sv_SE" // override auto-detected ru_RU with sv_SE
            dmg.licenseLanguageName: "Swedish, not Russian"
            dmg.licenseAgreeButtonText: "Of course"
            dmg.licenseDisagreeButtonText: "Never!"
            dmg.licensePrintButtonText: "Make Paper"
            dmg.licenseSaveButtonText: "Make Bits"
            dmg.licenseInstructionText: "Do please agree to the license!"
        }

        Group {
            files: ["*.lproj/**"]
            excludeFiles: ["ru_RU.lproj/eula.txt"]
        }
    }
}
