import qbs 1.0

Project {
    CppApplication {
        name: "main"
        install: true
        files: [
            "main.cpp",
            "myapp.desktop",
            "myapp.appdata.xml",
        ]

        Depends { name: "freedesktop" }

        freedesktop.name: "My App"
        freedesktop.desktopKeys: ({
            'Icon': "myapp.png"
        })

        Group {
            files: "myapp.png"
            fileTags: "freedesktop.appIcon"
        }
    }
}
