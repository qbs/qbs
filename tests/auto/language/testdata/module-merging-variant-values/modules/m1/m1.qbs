Module {
    condition: qbs.architecture === "a1" || qbs.architecture === "a2"

    property string arch
    qbs.architecture: undefined // We do something like this in GenericGCC.qbs
}
