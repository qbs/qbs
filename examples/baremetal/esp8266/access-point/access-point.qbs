CppApplication {
    condition: qbs.toolchain.contains("gcc") && qbs.architecture === "xtensa"

    Depends { name: "esp8266_sdk" }

    cpp.cLanguageVersion: "c99"
    cpp.positionIndependentCode: false

    // This required because ESP8266 SDK includes this 'user_config.h' internally.
    cpp.includePaths: ["."]

    cpp.staticLibraries: [
        "c",
        "crypto",
        "gcc",
        "lwip",
        "main",
        "net80211",
        "phy",
        "pp",
        "wpa"
    ]

    files: [
        "user_config.h",
        "user_main.c"
    ]
}
