Project {
    Library {
        Depends { name: "cpp" }
        Depends { name: "bundle" }

        property bool isShallow: {
            console.info("isShallow: " + bundle.isShallow);
            return bundle.isShallow;
        }

        Group {
            condition: qbs.targetOS.includes("darwin")
            files: ["PrivacyInfo.xcprivacy"]
            fileTags: ["bundle.input.privacymanifest"]
        }

        name: "Lib"
        bundle.isBundle: true
        bundle.publicHeaders: ["Lib.h"]
        files: ["Lib.cpp"].concat(bundle.publicHeaders || [])
    }
}