Project {
    property string qbsVersion
    property int qbsVersionMajor
    property int qbsVersionMinor
    property int qbsVersionPatch

    Product {
        property bool dummy: {
            if (qbsVersion !== qbs.version ||
                qbsVersionMajor !== qbs.versionMajor ||
                qbsVersionMinor !== qbs.versionMinor ||
                qbsVersionPatch !== qbs.versionPatch)
                throw("expected "
                    + [qbsVersion, qbsVersionMajor, qbsVersionMinor, qbsVersionPatch].join(", ")
                    + ", got "
                    + [qbs.version, qbs.versionMajor, qbs.versionMinor, qbs.versionPatch].join(", "));
            return false;
        }
    }
}
