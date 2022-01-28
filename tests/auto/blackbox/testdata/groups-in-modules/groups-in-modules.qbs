import qbs.Host

Project {
    Product {
        condition: {
            var result = qbs.targetPlatform === Host.platform();
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        Depends { name: "dep" }
        Depends { name: "helper" }
        Depends {
            name: "helper3"
            required: false
        }
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
