set(SOURCES
    dotgraph.cpp
    graphvizgenerator.cpp
    graphvizgeneratorplugin.cpp
)

add_qbs_plugin(graphvizgenerator
    DEPENDS qbscore
    SOURCES
        ${SOURCES}
)
