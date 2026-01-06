import qbs.Host

Project {
    property bool dummy: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
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
