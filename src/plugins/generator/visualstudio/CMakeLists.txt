set(SOURCES
    msbuildfiltersproject.cpp
    msbuildfiltersproject.h
    msbuildqbsgenerateproject.cpp
    msbuildqbsgenerateproject.h
    msbuildqbsproductproject.cpp
    msbuildqbsproductproject.h
    msbuildsharedsolutionpropertiesproject.cpp
    msbuildsharedsolutionpropertiesproject.h
    msbuildsolutionpropertiesproject.cpp
    msbuildsolutionpropertiesproject.h
    msbuildtargetproject.cpp
    msbuildtargetproject.h
    msbuildutils.h
    visualstudiogenerator.cpp
    visualstudiogenerator.h
    visualstudioguidpool.cpp
    visualstudioguidpool.h
    visualstudiogeneratorplugin.cpp
    )

add_qbs_plugin(visualstudiogenerator
    DEPENDS qbscore qbsmsbuild qbsjson
    SOURCES ${SOURCES}
    )
