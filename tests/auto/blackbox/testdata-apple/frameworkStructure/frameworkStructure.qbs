Project {
    property bool includeHeaders: true
    Library {
        Depends { name: "cpp" }
        Depends { name: "bundle" }

        property bool isShallow: {
            console.info("isShallow: " + bundle.isShallow);
            return bundle.isShallow;
        }

        name: "Widget"
        bundle.isBundle: true
        bundle.publicHeaders: project.includeHeaders ? ["Widget.h"] : undefined
        bundle.privateHeaders: project.includeHeaders ? ["WidgetPrivate.h"] : base
        bundle.resources: ["BaseResource", "en.lproj/EnglishResource"]
        files: ["Widget.cpp"].concat(bundle.publicHeaders || []).concat(bundle.privateHeaders || [])
    }
}
