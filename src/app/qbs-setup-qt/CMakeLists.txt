set(SOURCES
    commandlineparser.cpp
    commandlineparser.h
    main.cpp
    setupqt.cpp
    setupqt.h
    )

add_qbs_app(qbs-setup-qt
    DEPENDS qbscore qbsconsolelogger
    SOURCES ${SOURCES}
    )
