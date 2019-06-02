CppApplication {
    Depends { name: "flatbuf.c"; required: false }

    name: "monster_c"
    consoleApplication: true
    condition: flatbuf.c.present && qbs.targetPlatform === qbs.hostPlatform

    files: [
        "monster.c",
        "../shared/monster.fbs"
    ]
}
