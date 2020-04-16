CppApplication {
    consoleApplication: true
    cpp.defines: [
        'HOST_ARCHITECTURE="' + qbs.hostArchitecture + '"',
        'HOST_PLATFORM="' + qbs.hostPlatform + '"'
    ]
    files: "main.cpp"
}
