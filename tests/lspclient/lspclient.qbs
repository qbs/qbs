QbsApp {
    name: "qbs_lspclient"

    Depends { name: "qtclsp" }
    Depends { name: "Qt.network" }

    cpp.defines: base.filter(function(d) { return d !== "QT_NO_CAST_FROM_ASCII"; })

    files: "lspclient.cpp"
}
