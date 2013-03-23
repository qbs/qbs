Product {
    type: qbs.targetPlatform.indexOf("darwin") !== -1 ? "applicationbundle" : "application"
}
