var Utilities = require("qbs.Utilities");

// Typically, multiple architectures are used for migration from "old" arch to a "new" one
// For example: x86 -> x86_64 on macOS, armv7 -> arm64 on iOS

function enableOldArch(qbs, xcodeVersion) {
    return qbs.targetOS.contains("macos")
            && xcodeVersion
            && Utilities.versionCompare(xcodeVersion, "10") < 0
            || qbs.targetOS.contains("ios")
}

function getNewArch(qbs) {
    if (qbs.targetOS.contains("macos"))
        return "x86_64"
    else if (qbs.targetOS.contains("ios"))
        return "arm64"
    else if (qbs.targetOS.contains("tvos"))
        return "arm64"
    else if (qbs.targetOS.contains("watchos"))
        return "armv7k"
    throw "unsupported targetOS: " + qbs.targetOS;
}

function getOldArch(qbs) {
    if (qbs.targetOS.contains("macos"))
        return "x86"
    else if (qbs.targetOS.contains("ios"))
        return "armv7a"
    throw "unsupported targetOS: " + qbs.targetOS;
}

function getArchitectures(qbs, xcodeVersion) {
    return enableOldArch(qbs, xcodeVersion)
            ? [getOldArch(qbs), getNewArch(qbs)]
            : [getNewArch(qbs)];
}
