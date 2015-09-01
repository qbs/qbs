import qbs

Product {
    Depends { name: "java"; required: false }
    Probe {
        configure: {
            if (java.present) {
                var tools = {
                    "javac": java.compilerFilePath,
                    "java": java.interpreterFilePath,
                    "jar": java.jarFilePath
                };
                print(JSON.stringify(tools, undefined, 4));
            }
        }
    }
}
