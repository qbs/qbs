import qbs 1.0
import qbs.Probes

Module {
    name: 'definition'
    Depends { name: 'cpp' }
    Probes.IncludeProbe {
        id: includeNode
        names: "openssl/sha.h"
    }
    cpp.defines: includeNode.found ? 'TEXT="' + includeNode.path + '"' : undefined
}
