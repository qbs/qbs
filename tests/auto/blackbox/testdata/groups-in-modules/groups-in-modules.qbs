import qbs.Host

Project {
    Product {
        condition: {
            var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
            if (!result)
                console.info("target platform/arch differ from host platform/arch");
            return result;
        }
        Depends { name: "dep" }
        Depends { name: "helper" }
        Depends {
            name: "helper3"
            required: false
        }
        Depends { name: "helper7" }
        helper7.fileName: "helper7.c"

        Depends { name: "helper8" }

        type: ["diamond"]

        files: [
            "rock.coal"
        ]
    }

    Product {
        name: "dep"
        Export {
            Depends { name: "helper4" }
        }
    }
}
