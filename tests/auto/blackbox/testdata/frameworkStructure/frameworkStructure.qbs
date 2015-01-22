import qbs

Project {
    Library {
        Depends { name: "cpp" }

        name: "Widget"
        bundle.publicHeaders: ["Widget.h"]
        bundle.privateHeaders: ["WidgetPrivate.h"]
        bundle.resources: ["BaseResource", "en.lproj/EnglishResource"]
        files: ["Widget.cpp"].concat(bundle.publicHeaders).concat(bundle.privateHeaders)
    }
}
