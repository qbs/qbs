Library {
    property stringList publicHeaders
    property stringList privateHeaders

    property string headersDirectoryName: targetName
    property string publicHeadersDirectory: installpaths.headers + "/" + headersDirectoryName
    property string privateHeadersDirectory:
        installpaths.headers + "/" + headersDirectoryName + "/private"

    Depends { name: "cpp" }
    Depends { name: "installpaths" }

    Group {
        name: "Public Headers"
        files: publicHeaders
        fileTags: ["bundle.input.public_hpp", "hpp"]
        prefix: product.sourceDirectory + "/"
        qbs.install: install && !(isForDarwin && bundle.isBundle)
        qbs.installDir: publicHeadersDirectory
    }

    Group {
        name: "Private Headers"
        files: privateHeaders
        fileTags: ["bundle.input.private_hpp", "hpp"]
        prefix: product.sourceDirectory + "/"
        qbs.install: install && !(isForDarwin && bundle.isBundle)
        qbs.installDir: privateHeadersDirectory
    }
}
