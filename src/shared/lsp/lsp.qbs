import qbs.Utilities

QbsStaticLibrary {
    name: "qtclsp"

    Depends { name: "qbscore"; cpp.link: false }

    Depends {
        condition: Utilities.versionCompare(Qt.core.version, "6") >= 0
        name: "Qt.core5compat"
    }

    cpp.defines: base.filter(function(d) { return d !== "QT_NO_CAST_FROM_ASCII"; })
         .concat("LANGUAGESERVERPROTOCOL_STATIC_LIBRARY")

    files: [
        "algorithm.h",
        "basemessage.cpp",
        "basemessage.h",
        "callhierarchy.cpp",
        "callhierarchy.h",
        "client.cpp",
        "client.h",
        "clientcapabilities.cpp",
        "clientcapabilities.h",
        "completion.cpp",
        "completion.h",
        "diagnostics.cpp",
        "diagnostics.h",
        "filepath.h",
        "initializemessages.cpp",
        "initializemessages.h",
        "jsonkeys.h",
        "jsonobject.cpp",
        "jsonobject.h",
        "jsonrpcmessages.cpp",
        "jsonrpcmessages.h",
        "languagefeatures.cpp",
        "languagefeatures.h",
        "languageserverprotocol_global.h",
        "languageserverprotocoltr.h",
        "link.cpp",
        "link.h",
        "lsptypes.cpp",
        "lsptypes.h",
        "lsputils.cpp",
        "lsputils.h",
        "messages.cpp",
        "messages.h",
        "predicates.h",
        "progresssupport.cpp",
        "progresssupport.h",
        "semantictokens.cpp",
        "semantictokens.h",
        "servercapabilities.cpp",
        "servercapabilities.h",
        "shutdownmessages.cpp",
        "shutdownmessages.h",
        "textsynchronization.cpp",
        "textsynchronization.h",
        "textutils.cpp",
        "textutils.h",
        "workspace.cpp",
        "workspace.h",
    ]

    Export {
        Depends { name: "cpp" }
        cpp.defines: "LANGUAGESERVERPROTOCOL_STATIC_LIBRARY"
        cpp.includePaths: exportingProduct.sourceDirectory + "/.."
    }
}
