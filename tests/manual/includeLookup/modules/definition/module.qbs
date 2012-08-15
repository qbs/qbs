import qbs.base 1.0
import qbs.probes as Probes

Module {
    name: 'definition'
    Depends { name: 'cpp' }
    Probes.IncludeProbe {
        id: includeNode
        names: "openssl/sha.h"
    }
    cpp.defines: includeNode.found ? 'TEXT="' + includeNode.path + '"' : undefined
}
