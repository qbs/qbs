import qbs 1.0

CppApplication {
    Depends { condition: product.condition; name: "ib" }
    condition: qbs.targetOS.contains("osx")
    type: "applicationbundle"
    name: "CocoaApplication"

    cpp.precompiledHeader: "CocoaApplication-Prefix.pch"

    cpp.infoPlistFile: "CocoaApplication/CocoaApplication-Info.plist"

    cpp.frameworks: ["Cocoa"]

    Group {
        prefix: "CocoaApplication/"
        files: [
            "AppDelegate.h",
            "AppDelegate.m",
            "main.m"
        ]
    }

    Group {
        name: "Supporting Files"
        prefix: "CocoaApplication/en.lproj/"
        files: [
            "Credits.rtf",
            "InfoPlist.strings",
            "MainMenu.xib"
        ]
    }
}
