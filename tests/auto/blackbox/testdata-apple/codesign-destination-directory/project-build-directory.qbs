Project {
    CppApplication {
        name: "app"
        files: ["main.c"]
        codesign.enableCodeSigning: true
        codesign.signingType: "ad-hoc"
        destinationDirectory: project.buildDirectory
    }
}
