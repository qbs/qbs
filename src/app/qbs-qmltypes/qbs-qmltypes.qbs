import qbs 1.0
import "../apptemplate.qbs" as QbsApp

QbsApp {
    name: "qbs-qmltypes"
    files: [
        "../shared/qbssettings.h",
        "main.cpp"
    ]
}

