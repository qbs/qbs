set(SOURCES
    android-setup.cpp
    android-setup.h
    commandlineparser.cpp
    commandlineparser.h
    main.cpp
    )

add_qbs_app(qbs-setup-android
    DEPENDS qbscore qbsconsolelogger
    SOURCES ${SOURCES}
    )
