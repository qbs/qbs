set(SOURCES
    configcommand.h
    configcommandexecutor.cpp
    configcommandexecutor.h
    configcommandlineparser.cpp
    configcommandlineparser.h
    configmain.cpp
    )

add_qbs_app(qbs-config
    DEPENDS qbscore qbsconsolelogger
    SOURCES ${SOURCES}
    )
