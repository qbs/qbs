Application {
    qbs.targetPlatform: "android"
    name: "com.example.android.basicmediadecoder"

    Android.sdk.sourceSetDir: Android.sdk.sdkDir
            + "/samples/android-BasicMediaDecoder/Application/src/main"
    Android.sdk.versionCode: 5
    Android.sdk.versionName: "5.0"
}
