import qbs

Project {
    property stringList bundleFileTags: ["bundle.content"]

    property stringList buildableProducts: ["A", "B", "C", "D", "E", "F", "G"]

    Application {
        Depends { name: "cpp" }
        Depends { name: "B" }
        Depends { name: "C" }
        Depends { name: "D" }
        condition: buildableProducts.contains("A")
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

    Application {
        Depends { name: "cpp" }
        Depends { name: "B" }
        Depends { name: "C" }
        Depends { name: "D" }
        condition: buildableProducts.contains("ABadApple")
        name: "ABadApple"
        bundle._productTypeIdentifier: "com.apple.product-type.will.never.exist.ever.guaranteed"
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

    Application {
        Depends { name: "cpp" }
        Depends { name: "B" }
        Depends { name: "C" }
        Depends { name: "D" }
        condition: buildableProducts.contains("ABadThirdParty")
        name: "ABadThirdParty"
        bundle._productTypeIdentifier: "org.special.third.party.non.existent.product.type"
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
        condition: buildableProducts.containsAny(["A", "B", "ABadApple", "ABadThirdParty"])
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
        condition: buildableProducts.containsAny(["A", "C", "ABadApple", "ABadThirdParty"])
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
        condition: buildableProducts.containsAny(["A", "D", "ABadApple", "ABadThirdParty"])
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
        condition: buildableProducts.contains("E")
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
        condition: buildableProducts.contains("F")
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
        condition: buildableProducts.contains("G")
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
