CppApplication {
    name: "p"
    property bool fallbacksEnabled
    Depends { name: "pkgconfig"; required: false }
    Depends { name: "qbsmetatestmodule"; required: false; enableFallback: fallbacksEnabled }
    property bool dummy: { console.info("pkg-config present: " + pkgconfig.present); }
    files: "main.cpp"
}
