set(SOLUTION_SOURCES
    ivisualstudiosolutionproject.cpp
    ivisualstudiosolutionproject.h
    visualstudiosolutionfileproject.cpp
    visualstudiosolutionfileproject.h
    visualstudiosolutionfolderproject.cpp
    visualstudiosolutionfolderproject.h
    visualstudiosolution.cpp
    visualstudiosolution.h
    visualstudiosolutionglobalsection.cpp
    visualstudiosolutionglobalsection.h
    )
list_transform_prepend(SOLUTION_SOURCES solution/)

set(MSBUILD_SOURCES
    imsbuildgroup.cpp
    imsbuildgroup.h
    imsbuildnode.cpp
    imsbuildnode.h
    imsbuildnodevisitor.h
    msbuildimport.cpp
    msbuildimport.h
    msbuildimportgroup.cpp
    msbuildimportgroup.h
    msbuilditem.cpp
    msbuilditem.h
    msbuilditemdefinitiongroup.cpp
    msbuilditemdefinitiongroup.h
    msbuilditemgroup.cpp
    msbuilditemgroup.h
    msbuilditemmetadata.cpp
    msbuilditemmetadata.h
    msbuildproject.cpp
    msbuildproject.h
    msbuildproperty.cpp
    msbuildproperty.h
    msbuildpropertybase.cpp
    msbuildpropertybase.h
    msbuildpropertygroup.cpp
    msbuildpropertygroup.h
    )
list_transform_prepend(MSBUILD_SOURCES msbuild/)

set(MSBUILD_ITEMS_SOURCES
    msbuildclcompile.cpp
    msbuildclcompile.h
    msbuildclinclude.cpp
    msbuildclinclude.h
    msbuildfileitem.cpp
    msbuildfileitem.h
    msbuildfilter.cpp
    msbuildfilter.h
    msbuildlink.cpp
    msbuildlink.h
    msbuildnone.cpp
    msbuildnone.h
    )
list_transform_prepend(MSBUILD_ITEMS_SOURCES msbuild/items/)

set(IO_SOURCES
    msbuildprojectwriter.cpp
    msbuildprojectwriter.h
    visualstudiosolutionwriter.cpp
    visualstudiosolutionwriter.h
    )
list_transform_prepend(IO_SOURCES io/)

add_qbs_library(qbsmsbuild
    STATIC
    DEPENDS qbscore
    SOURCES ${SOLUTION_SOURCES} ${MSBUILD_SOURCES} ${MSBUILD_ITEMS_SOURCES} ${IO_SOURCES}
    )
