function characterSetDefines(charset) {
    var defines = [];
    if (charset === "unicode")
        defines.push("UNICODE", "_UNICODE");
    else if (charset === "mbcs")
        defines.push("_MBCS");
    return defines;
}

function isValidWindowsVersion(systemVersion) {
    // Add new Windows versions to this list when they are released
    var realVersions = [ '6.2', '6.1', '6.0', '5.2', '5.1', '5.0', '4.0' ];
    for (i in realVersions)
        if (systemVersion === realVersions[i])
            return true;

    return false;
}

function getWindowsVersionInFormat(systemVersion, format) {
    if (!isValidWindowsVersion(systemVersion))
        return undefined;

    var major = parseInt(systemVersion.split('.')[0]);
    var minor = parseInt(systemVersion.split('.')[1]);

    if (format === 'hex') {
        return '0x' + major + (minor < 10 ? '0' : '') + minor;
    } else if (format === 'subsystem') {
        // http://msdn.microsoft.com/en-us/library/fcc1zstk.aspx
        return major + '.' + (minor < 10 ? '0' : '') + minor;
    } else {
        throw ("Unrecognized Windows version format " + format + ". Must be in {hex, subsystem}.");
    }
}
