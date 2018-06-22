Project {
    NodeJSApplication {
        Depends { name: "typescript" }
        Depends { name: "lib" }

        typescript.warningLevel: "pedantic"
        typescript.generateDeclarations: true
        typescript.moduleLoader: "commonjs"
        nodejs.applicationFile: "main.ts"

        name: "animals"

        files: [
            "animals.ts",
            "extra.js",
            "woosh/extra.ts"
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

    Product {
        Depends { name: "typescript" }

        typescript.generateDeclarations: true

        name: "lib2"

        files: [
            "foo2.ts"
        ]
    }

    NodeJSApplication {
        Depends { name: "typescript" }
        Depends { name: "lib2" }

        typescript.singleFile: true
        nodejs.applicationFile: "hello.ts"

        name: "single"
    }
}
