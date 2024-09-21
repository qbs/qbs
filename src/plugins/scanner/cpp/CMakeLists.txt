set(SOURCES
    ../scanner.h
    cpp_global.h
    cppscannerplugin.cpp
    )

add_qbs_plugin(qbs_cpp_scanner
    DEFINES "CPLUSPLUS_NO_PARSER"
    DEPENDS qbscore
    SOURCES ${SOURCES}
    )
