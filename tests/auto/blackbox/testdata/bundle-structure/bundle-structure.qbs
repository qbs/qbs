import qbs

Project {
    property stringList bundleFileTags: [
        "aggregate_infoplist", "pkginfo", "hpp",
        "icns", "resourcerules", "xcent",
        "compiled_ibdoc", "compiled_assetcatalog",
        "bundle.symlink.headers", "bundle.symlink.private-headers",
        "bundle.symlink.resources", "bundle.symlink.executable",
        "bundle.symlink.version", "bundle.hpp", "bundle.resource",
    ]

    Application {
        Depends { name: "cpp" }
        Depends { name: "B" }
        Depends { name: "C" }
        Depends { name: "D" }
        name: "A"
        bundle.isBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        files: ["dummy.c"]
        Group {
            fileTagsFilter: product.type.concat(project.bundleFileTags)
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        name: "B"
        bundle.isBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        files: ["dummy.c"]
        Group {
            fileTagsFilter: product.type.concat(project.bundleFileTags)
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }

    StaticLibrary {
        Depends { name: "cpp" }
        name: "C"
        bundle.isBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        files: ["dummy.c"]
        Group {
            fileTagsFilter: product.type.concat(project.bundleFileTags)
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }

    LoadableModule {
        Depends { name: "cpp" }
        name: "D"
        bundle.isBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        files: ["dummy.c"]
        Group {
            fileTagsFilter: product.type.concat(project.bundleFileTags)
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }

    ApplicationExtension {
        Depends { name: "cpp" }
        name: "E"
        bundle.isBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        files: ["dummy.c"]
        Group {
            fileTagsFilter: product.type.concat(project.bundleFileTags)
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }

    XPCService {
        Depends { name: "cpp" }
        name: "F"
        bundle.isBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        files: ["dummy.c"]
        Group {
            fileTagsFilter: product.type.concat(project.bundleFileTags)
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }

    Product {
        Depends { name: "bundle" }
        type: ["inapppurchase"]
        name: "G"
        bundle.isBundle: true
        bundle.resources: ["resource.txt"]
        Group {
            fileTagsFilter: product.type.concat(project.bundleFileTags)
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }
}
