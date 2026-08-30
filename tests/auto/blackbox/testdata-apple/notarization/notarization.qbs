Project {
    name: "p"

    property bool enableNotarization: false

    CppApplication {
        name: "A"
        version: "1.0.0"
        property bool canNotarize: notarization.keychainProfile && codesign._actualSigningIdentity
        condition: {
            if (!qbs.targetOS.includes("macos")) {
                console.info("Skip this test: not macOS");
                return false;
            }
            if (project.enableNotarization && !canNotarize) {
                console.info("Skip this test: missing credentials");
                return false;
            }
            return true;
        }
        bundle.isBundle: true
        files: "app.cpp"
        Depends { name: "notarization" }
        codesign.enableCodeSigning: true
        codesign.signingType: project.enableNotarization ? "apple-id" : "ad-hoc"
        // apple.qbs forces "--timestamp=none", which Apple's notary service rejects.
        codesign.signingTimestamp: project.enableNotarization ? "" : "none"
        codesign.codesignFlags: project.enableNotarization
                ? ["--options", "runtime", "--timestamp"] : []
        notarization.enableNotarization: project.enableNotarization && canNotarize
        install: true
        installDir: ""
    }
}
