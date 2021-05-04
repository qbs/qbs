import "base.qbs" as Base
import "debian.js" as Debian

Base {
    priority: 0
    condition: qbs.targetOS.contains("linux")

    lib: libdirProbe.found ? libdirProbe.libDir : base

    Probe {
        id: libdirProbe
        condition: autotetect

        property string libDir

        configure: {
            if (Debian.isDebian()) {
                libDir = Debian.getDebianLibPath(qbs.architecture);
            } else if (File.exists("/usr/lib64")) {
                libDir = "lib64";
            } else {
                libDir = "lib";
            }
            found = true;
        }
    }
}
