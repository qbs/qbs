import qbs

Project {
    NodeJSApplication {
        Depends { name: "typescript" }
        Depends { name: "lib" }

        typescript.warningLevel: ["pedantic"]
        typescript.generateDeclarations: true
        typescript.moduleLoader: "commonjs"
        nodejs.applicationFile: "main.ts"

        name: "animals"

        files: [
            "animals.ts",
            "extra.js",
            "main.ts"
        ]
    }

    Product {
        Depends { name: "typescript" }

        typescript.generateDeclarations: true
        typescript.moduleLoader: "commonjs"

        name: "lib"

        files: [
            "foo.ts"
        ]
    }
}
