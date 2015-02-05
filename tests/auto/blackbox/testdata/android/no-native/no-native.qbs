import qbs

AndroidApk {
    name: "com.example.android.basicmediadecoder"

    property string sourcesPrefix: Android.sdk.sdkDir
            + "/samples/android-21/media/BasicMediaDecoder/Application/src/main/"

    resourcesDir: sourcesPrefix + "/res"
    sourcesDir: sourcesPrefix + "/java"
    manifestFile: sourcesPrefix + "/AndroidManifest.xml"
}
