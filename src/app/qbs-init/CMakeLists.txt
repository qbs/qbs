set(SOURCES
    commandlineparser.cpp
    commandlineparser.h
    init-main.cpp
    inititem.cpp
    inititem.h
    templates/templates.qrc
    )

add_qbs_app(qbs-init
    DEPENDS qbscore qbsconsolelogger
    SOURCES ${SOURCES}
    )
