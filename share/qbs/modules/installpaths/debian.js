
var File = require("qbs.File");
var Process = require("qbs.Process");

const debianArchMap = {
    "arm": "arm",
    "arm64": "arm64",
    "hppa": "hppa",
    "ia64": "ia64",
    "mips": "mips",
    "mips64": "mips64",
    "ppc64": "ppc64",
    "riscv": "riscv64",
    "s390x": "s390x",
    "sparc": "sparc",
    "sparc64": "sparc64",
    "x86": "i386",
    "x86_64": "amd64",
};

function isDebian() {
    return File.exists("/etc/debian_version");
}

// logic yoinked from meson
function getDebianLibPath(qbsArchitecture) {
    var proc = new Process()

    const debianArch = debianArchMap[qbsArchitecture]
    if (debianArch === undefined)
        return "lib";
    const args = ["-qDEB_TARGET_MULTIARCH", "-A", debianArch];
    if (proc.exec("dpkg-architecture", args, true) == 0) {
        return "lib/" + proc.readStdOut().trim();
    }
    return "lib";
}
