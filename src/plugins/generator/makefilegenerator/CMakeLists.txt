set(SOURCES
    makefilegenerator.cpp
    makefilegenerator.h
    makefilegeneratorplugin.cpp
    )

add_qbs_plugin(makefilegenerator
    DEPENDS qbscore
    SOURCES ${SOURCES}
    )
