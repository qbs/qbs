import qbs

Project {
    property bool includeHeaders: true
    Library {
        Depends { name: "cpp" }

        name: "Widget"
        bundle.isBundle: true
        bundle.publicHeaders: project.includeHeaders ? ["Widget.h"] : undefined
        bundle.privateHeaders: project.includeHeaders ? ["WidgetPrivate.h"] : base
        bundle.resources: ["BaseResource", "en.lproj/EnglishResource"]
        files: ["Widget.cpp"].concat(bundle.publicHeaders || []).concat(bundle.privateHeaders || [])
    }
}
