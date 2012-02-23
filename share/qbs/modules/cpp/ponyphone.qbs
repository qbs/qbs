/*
    Giddyup, pony pony pony!

    How to setup this crazy horsey platform?
    - install the pony SDK
    - call the following
        qbs config --global ponyphone/substitute theLittlePrancingPony
        qbs config --global ponyphone/sdkInstallPath ~/theLittlePrancingPonySDK
    - stand up and sing the Mr. Ed theme song!
*/

import qbs.base 1.0

UnixGCC {
    property string substitute: qbs.configurationValue('ponyphone/substitute')

    condition: qbs.hostOS == 'linux' && qbs.targetOS == 'linux' && qbs.toolchain == 'gcc' && qbs.platform == 'ponyphone'

    toolchainPrefix: {
        if (architecture == 'i586')
            return architecture + '-' + substitute + '-linux-'
        else if (architecture == 'armv7hl')
            return architecture + '-' + substitute + '-linux-gnueabi-'
        throw "unknown target architecture: " + architecture
    }
    toolchainInstallPath: qbs.configurationValue('ponyphone/sdkInstallPath') + "/" + substitute + "/" + architecture + "/toolchain/bin"
}

