Product {
    property string bundleExtension
    type: qbs.targetPlatform.indexOf("darwin") !== -1 ? "applicationbundle" : "application"
}
