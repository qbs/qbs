set(PARSER_SOURCES
    commandlineoption.cpp
    commandlineoption.h
    commandlineoptionpool.cpp
    commandlineoptionpool.h
    commandlineparser.cpp
    commandlineparser.h
    commandpool.cpp
    commandpool.h
    commandtype.h
    parsercommand.cpp
    parsercommand.h
    )
list_transform_prepend(PARSER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/app/qbs/parser/")

add_qbs_test(cmdlineparser
    DEFINES
        "QBS_VERSION=\"${QBS_VERSION}\""
    INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../src"
    SOURCES
        tst_cmdlineparser.cpp ../../../src/app/qbs/qbstool.cpp ${PARSER_SOURCES}
    )
