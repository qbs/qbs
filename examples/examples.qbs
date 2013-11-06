import qbs

Project {
    references: [
        "app-and-lib/app_and_lib.qbs",
        "cocoa-application/CocoaApplication.qbs",
        "collidingmice/collidingmice.qbs",
        "helloworld-complex/hello.qbs",
        "helloworld-minimal/hello.qbs",
        "helloworld-qt/hello.qbs",
    ]
}
