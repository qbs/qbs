Project {
    CppApplication {
        name: "main"
        install: true
        property bool dummy: {
            console.info("is emscripten: " + qbs.toolchain.includes("emscripten"));
        }
        files: [
            "main.cpp",
            "myapp.desktop",
            "myapp.appdata.xml",
        ]

        Depends { name: "freedesktop" }

        freedesktop.appName: "My App"
        freedesktop.desktopKeys: ({
            'Icon': "myapp.png"
        })
        freedesktop.hicolorRoot: project.sourceDirectory + "/icons/"

        Group {
            name: "hicolor"
            prefix: project.sourceDirectory + "/icons/"
            files: [
                "48x48/apps/myapp.png",
                "48x48@2/apps/myapp.png",
                "64x64/apps/myapp.png",
                "64x64@2/apps/myapp.png",
                "64x64/mimetypes/application-format.png",
                "scalable/apps/myapp.png",
            ]
            fileTags: "freedesktop.appIcon"
        }
    }
}
