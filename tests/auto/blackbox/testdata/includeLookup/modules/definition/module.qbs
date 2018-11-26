import qbs.Probes

Module {
    name: 'definition'
    Depends { name: 'cpp' }
    property string modulePath: path
    Probes.IncludeProbe {
        id: includeNode
        names: "fakeopenssl/sha.h"
        platformSearchPaths: [modulePath]
    }
    cpp.defines: includeNode.found ? 'TEXT="' + includeNode.path + '"' : undefined
}
