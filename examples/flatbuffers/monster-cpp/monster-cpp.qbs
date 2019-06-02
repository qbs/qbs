CppApplication {
    Depends { name: "flatbuf.cpp"; required: false }

    name: "monster_cpp"
    consoleApplication: true
    condition: flatbuf.cpp.present && qbs.targetPlatform === qbs.hostPlatform

    files: [
        "monster.cpp",
        "../shared/monster.fbs"
    ]
}
