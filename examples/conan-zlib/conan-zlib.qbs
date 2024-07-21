//![0]

import qbs.Host

CppApplication {
    consoleApplication: true
    condition: zlib.present && qbs.targetPlatform === Host.platform()

    Depends { name: "cpp" }
    Depends { name: "zlib"; required: false }

    install: true
    qbs.installPrefix: ""

    files: "main.c"
    qbsModuleProviders: "conan"
}
//![0]
