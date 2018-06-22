Project {
    Application {
        Depends { name: "cli" }
        Depends { name: "HelloWorldModule"; condition: !qbs.toolchain.contains("mono") }
        Depends { name: "NetLib" }

        type: "application"
        name: "Hello"
        files: ["HelloWorld.cs"]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }

    // Mono's VB compiler doesn't support modules yet, and if we try with C#, it crashes anyways
    NetModule {
        condition: !qbs.toolchain.contains("mono")
        Depends { name: "cli" }

        name: "HelloWorldModule"

        files: ["Module.vb"]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }

    DynamicLibrary {
        Depends { name: "cli" }

        name: "NetLib"
        files: ["Libby.cs", "Libby2.cs"]

        // fill-in for missing NetModule
        Group {
            condition: qbs.toolchain.contains("mono")
            files: ["Module.cs"]
        }

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }
}
