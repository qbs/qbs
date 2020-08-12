set(SOURCES
    coloredoutput.cpp
    coloredoutput.h
    consolelogger.cpp
    consolelogger.h
   )

add_qbs_library(logginglib
    STATIC
    DEPENDS corelib
    PUBLIC_INCLUDES $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../>
    SOURCES ${SOURCES}
    )
