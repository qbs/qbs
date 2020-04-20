CppApplication {
    Depends { name: "ib" }
    Depends { name: "bundle" }
    property bool isShallow: {
        console.info("isShallow: " + bundle.isShallow);
        return bundle.isShallow;
    }
    files: ["main.c", "white.iconset"]
}
