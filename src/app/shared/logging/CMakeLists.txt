set(SOURCES
    coloredoutput.cpp
    coloredoutput.h
    consolelogger.cpp
    consolelogger.h
   )

add_qbs_library(qbsconsolelogger
    STATIC
    DEPENDS qbscore
    PUBLIC_INCLUDES $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    SOURCES ${SOURCES}
    )
