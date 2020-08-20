set(SOURCES
    coloredoutput.cpp
    coloredoutput.h
    consolelogger.cpp
    consolelogger.h
   )

add_qbs_library(qbsconsolelogger
    STATIC
    DEPENDS qbscore
    SOURCES ${SOURCES}
    )
