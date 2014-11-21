import qbs
import qbs.File

Product {
    type: ["android.apk"]
    Depends { name: "Android.sdk" }

    property string packageName: name
    property bool automaticSources: true
    property path resourcesDir: "res"
    property path assetsDir: "assets"
    property path sourcesDir: "src"
    property path manifestFile: "AndroidManifest.xml"

    Group {
        name: "java sources"
        condition: product.automaticSources
        prefix: product.sourcesDir + '/'
        files: "**/*.java"
    }

    Group {
        name: "android resources"
        condition: product.automaticSources
        fileTags: ["android.resources"]
        prefix: product.resourcesDir + '/'
        files: "**/*"
    }

    Group {
        name: "android assets"
        condition: product.automaticSources
        fileTags: ["android.assets"]
        prefix: product.assetsDir + '/'
        files: "**/*"
    }

    Group {
        name: "manifest"
        condition: product.automaticSources
        files: [manifestFile]
    }
}
