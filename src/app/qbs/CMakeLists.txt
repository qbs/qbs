set(SOURCES
    application.cpp
    application.h
    commandlinefrontend.cpp
    commandlinefrontend.h
    consoleprogressobserver.cpp
    consoleprogressobserver.h
    ctrlchandler.cpp
    ctrlchandler.h
    main.cpp
    qbstool.cpp
    qbstool.h
    session.cpp
    session.h
    sessionpacket.cpp
    sessionpacket.h
    sessionpacketreader.cpp
    sessionpacketreader.h
    status.cpp
    status.h
    stdinreader.cpp
    stdinreader.h
    )

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
list_transform_prepend(PARSER_SOURCES parser/)

add_qbs_app(qbs
    DEFINES
        "QBS_VERSION=\"${QBS_VERSION}\""
        "QBS_RELATIVE_LIBEXEC_PATH=\"${QBS_RELATIVE_LIBEXEC_PATH}\""
        "QBS_RELATIVE_SEARCH_PATH=\"${QBS_RELATIVE_SEARCH_PATH}\""
        "QBS_RELATIVE_PLUGINS_PATH=\"${QBS_RELATIVE_PLUGINS_PATH}\""
    DEPENDS qbscore qbsconsolelogger
    SOURCES ${SOURCES} ${PARSER_SOURCES}
    )

add_dependencies(qbs qbs_cpp_scanner qbs_qt_scanner)
