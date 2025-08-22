var Utilities = require("qbs.Utilities");

function stripKernelReleaseSuffix(r) {
    var suffixes = ["-RELEASE", "-STABLE", "-CURRENT"];
    for (var i = 0; i < suffixes.length; i++) {
        var idx = r.indexOf(suffixes[i]);
        if (idx >= 0)
            return r.substr(0, idx);
    }
    return r;
}

function hostKernelRelease() {
    return stripKernelReleaseSuffix(Utilities.kernelVersion());
}
