add_qbs_test(language
    DEFINES
        "QBS_VERSION=\"${QBS_VERSION}\""
    DEPENDS
        qbsquickjsheaders
        Qt6Core5Compat
    SOURCES
        tst_language.cpp
        tst_language.h
    )
