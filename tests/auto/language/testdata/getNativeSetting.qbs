import qbs.FileInfo
import qbs.Utilities

Project {
    Product {
        name: {
            if (qbs.hostOS.contains("macos")) {
                return Utilities.getNativeSetting("/System/Library/CoreServices/SystemVersion.plist", "ProductName");
            } else if (qbs.hostOS.contains("windows")) {
                var productName = Utilities.getNativeSetting("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows NT\\CurrentVersion", "ProductName");
                if (productName.contains("Windows")) {
                    return "Windows";
                }
                return undefined;
            } else {
                return Utilities.getNativeSetting(FileInfo.joinPaths(path, "nativesettings.ini"), "osname");
            }
        }
    }

    Product {
        name: Utilities.getNativeSetting("/dev/null", undefined, "fallback");
    }
}
