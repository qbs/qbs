Project {
    property stringList bundleFileTags: ["bundle.content"]

    property stringList buildableProducts: ["A", "B", "C", "D", "E", "F", "G"]

    Product {
        Depends { name: "bundle" }
        condition: {
            console.info("bundle.isShallow: " + bundle.isShallow);
            console.info("qbs.targetOS: " + qbs.targetOS);
            return false;
        }
    }

    Application {
        Depends { name: "cpp" }
        Depends { name: "B" }
        Depends { name: "C" }
        Depends { name: "D" }
        condition: buildableProducts.includes("A")
        name: "A"
        bundle.isBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        files: ["dummy.c"]
        install: true
        installDir: ""
    }

    Application {
        Depends { name: "cpp" }
        Depends { name: "B" }
        Depends { name: "C" }
        Depends { name: "D" }
        condition: buildableProducts.includes("ABadApple")
        name: "ABadApple"
        bundle._productTypeIdentifier: "com.apple.product-type.will.never.exist.ever.guaranteed"
        bundle.isBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        files: ["dummy.c"]
        install: true
        installDir: ""
    }

    Application {
        Depends { name: "cpp" }
        Depends { name: "B" }
        Depends { name: "C" }
        Depends { name: "D" }
        condition: buildableProducts.includes("ABadThirdParty")
        name: "ABadThirdParty"
        bundle._productTypeIdentifier: "org.special.third.party.non.existent.product.type"
        bundle.isBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        files: ["dummy.c"]
        install: true
        installDir: ""
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
        install: true
        installDir: ""
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
        install: true
        installDir: ""
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
        install: true
        installDir: ""
    }

    ApplicationExtension {
        Depends { name: "cpp" }
        condition: buildableProducts.includes("E")
        name: "E"
        bundle.isBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        files: ["dummy.c"]
        install: true
        installDir: ""
    }

    XPCService {
        Depends { name: "cpp" }
        condition: buildableProducts.includes("F")
        name: "F"
        bundle.isBundle: true
        bundle.publicHeaders: ["dummy.h"]
        bundle.privateHeaders: ["dummy_p.h"]
        bundle.resources: ["resource.txt"]
        files: ["dummy.c"]
        install: true
        installDir: ""
    }

    Product {
        Depends { name: "bundle" }
        condition: buildableProducts.includes("G")
        type: ["inapppurchase"]
        name: "G"
        bundle.isBundle: true
        bundle.resources: ["resource.txt"]
        // XCode 12.5 does not support com.apple.product-type.in-app-purchase-content type anymore,
        // so use older specs from Qbs
        bundle._useXcodeBuildSpecs: false
        Group {
            fileTagsFilter: product.type.concat(project.bundleFileTags)
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }
}
