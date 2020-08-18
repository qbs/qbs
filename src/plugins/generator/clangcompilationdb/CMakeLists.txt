set(SOURCES
    clangcompilationdbgenerator.cpp
    clangcompilationdbgenerator.h
    clangcompilationdbgeneratorplugin.cpp
    )

add_qbs_plugin(clangcompilationdbgenerator
    DEPENDS qbscore
    SOURCES ${SOURCES}
    )
