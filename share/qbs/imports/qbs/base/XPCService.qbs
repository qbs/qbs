import qbs

Application {
    type: base.concat(["xpcservice"])

    condition: qbs.targetOS.contains("darwin")

    property string xpcServiceType: "Application"

    bundle.infoPlist: {
        var infoPlist = base;
        if (xpcServiceType) {
            infoPlist["XPCService"] = {
                "ServiceType": xpcServiceType
            };
        }
        return infoPlist;
    }
}
