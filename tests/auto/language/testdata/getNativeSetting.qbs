import qbs.FileInfo
import qbs.Utilities

import qbs.Host

Project {
    Product {
        name: "p1"
        targetName: {
            if (Host.os().includes("macos")) {
                return Utilities.getNativeSetting("/System/Library/CoreServices/SystemVersion.plist", "ProductName");
            } else if (Host.os().includes("windows")) {
                var productName = Utilities.getNativeSetting("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows NT\\CurrentVersion", "ProductName");
                if (productName.includes("Windows")) {
                    return "Windows";
                }
                return undefined;
            } else {
                return Utilities.getNativeSetting(FileInfo.joinPaths(path, "nativesettings.ini"), "osname");
            }
        }
    }

    Product {
        name: "p2"
        targetName: Utilities.getNativeSetting("/dev/null", undefined, "fallback");
    }
}
