import qbs

Project {
    condition: withExamples
    references: [
        "cocoa-application/CocoaApplication.qbs",
        "collidingmice/collidingmice.qbs",
        "helloworld-complex/hello.qbs",
        "helloworld-minimal/hello.qbs",
        "helloworld-qt/hello.qbs",
    ]
}
