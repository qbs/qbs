import qbs.File
import qbs.FileInfo

Product {
    name: "undefined-target-platform"
    qbs.targetPlatform: undefined

    readonly property bool _validate: {
        if ((qbs.targetOS instanceof Array) && qbs.targetOS.length === 0)
            return true;
        throw "Invalid qbs.targetOS value: " + qbs.targetOS;
    }
}
