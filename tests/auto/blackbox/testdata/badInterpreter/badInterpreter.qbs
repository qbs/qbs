import qbs.Host

Project {
    property bool enabled: {
        var result = qbs.targetPlatform === Host.platform();
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result;
    }

    qbsSearchPaths: base.concat(["qbs"])

    Product {
        Depends { name: "script-test" }
        name: "script-ok"
        type: ["application"]

        Group {
            files: [product.name]
            fileTags: ["script"]
        }
    }

    Product {
        Depends { name: "script-test" }
        name: "script-noexec"
        type: ["application"]

        Group {
            files: [product.name]
            fileTags: ["script"]
        }
    }

    Product {
        Depends { name: "script-test" }
        name: "script-interp-missing"
        type: ["application"]

        Group {
            files: [product.name]
            fileTags: ["script"]
        }
    }

    Product {
        Depends { name: "script-test" }
        name: "script-interp-noexec"
        type: ["application"]

        Group {
            files: [product.name]
            fileTags: ["script"]
        }
    }
}
