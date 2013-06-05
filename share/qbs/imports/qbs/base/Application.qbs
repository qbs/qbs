Product {
    property string bundleExtension
    type: qbs.targetOS.contains("darwin") ? "applicationbundle" : "application"
}
