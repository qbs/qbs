CppApplication {
    Depends { name: "ib" }
    Depends { name: "bundle" }
    property bool isShallow: {
        console.info("bundle.isShallow: " + bundle.isShallow);
        return bundle.isShallow;
    }
    files: ["main.c", "AppIcon.icon"]
    ib.appIconName: "AppIcon"
}
