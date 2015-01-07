import qbs

XPCService {
    type: base.concat(["applicationextension"])

    cpp.entryPoint: "_NSExtensionMain"
    cpp.frameworks: base.concat(["/System/Library/PrivateFrameworks/PlugInKit.framework"])
    cpp.requireAppExtensionSafeApi: true

    xpcServiceType: undefined
    property var extensionAttributes
    property string extensionPointIdentifier
    property string extensionPrincipalClass

    bundle.infoPlist: {
        var infoPlist = base;
        infoPlist["NSExtension"] = {
            "NSExtensionAttributes": extensionAttributes || {},
            "NSExtensionPointIdentifier": extensionPointIdentifier,
            "NSExtensionPrincipalClass": extensionPrincipalClass
        };
        return infoPlist;
    }
}
