import qbs.FileInfo

Project {
    Product {
        name: {
            if (qbs.hostOS.contains("osx")) {
                return qbs.getNativeSetting("/System/Library/CoreServices/SystemVersion.plist", "ProductName");
            } else if (qbs.hostOS.contains("windows")) {
                var productName = qbs.getNativeSetting("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows NT\\CurrentVersion", "ProductName");
                if (productName.contains("Windows")) {
                    return "Windows";
                }
                return undefined;
            } else {
                return qbs.getNativeSetting(FileInfo.joinPaths(path, "nativesettings.ini"), "osname");
            }
        }
    }

    Product {
        name: qbs.getNativeSetting("/dev/null", undefined, "fallback");
    }
}
