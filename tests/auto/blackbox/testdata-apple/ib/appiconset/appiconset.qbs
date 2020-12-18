CppApplication {
    Depends { name: "ib" }
    Depends { name: "bundle" }
    property bool isShallow: {
        console.info("bundle.isShallow: " + bundle.isShallow);
        return bundle.isShallow;
    }
    files: ["main.c", "AppIconSet.xcassets"]
    ib.appIconName: "AppIcon"
}
