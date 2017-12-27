TARGET = qbsmsbuild
include(../staticlibrary.pri)
include(../corelib/use_corelib.pri)

HEADERS += \
    io/msbuildprojectwriter.h \
    io/visualstudiosolutionwriter.h \
    msbuild/imsbuildgroup.h \
    msbuild/imsbuildnode.h \
    msbuild/imsbuildnodevisitor.h \
    msbuild/imsbuildproperty.h \
    msbuild/items/msbuildclcompile.h \
    msbuild/items/msbuildclinclude.h \
    msbuild/items/msbuildfileitem.h \
    msbuild/items/msbuildfilter.h \
    msbuild/items/msbuildlink.h \
    msbuild/items/msbuildnone.h \
    msbuild/msbuildimport.h \
    msbuild/msbuildimportgroup.h \
    msbuild/msbuilditem.h \
    msbuild/msbuilditemdefinitiongroup.h \
    msbuild/msbuilditemgroup.h \
    msbuild/msbuilditemmetadata.h \
    msbuild/msbuildproject.h \
    msbuild/msbuildproperty.h \
    msbuild/msbuildpropertygroup.h \
    solution/ivisualstudiosolutionproject.h \
    solution/visualstudiosolution.h \
    solution/visualstudiosolutionfileproject.h \
    solution/visualstudiosolutionfolderproject.h \
    solution/visualstudiosolutionglobalsection.h

SOURCES += \
    io/msbuildprojectwriter.cpp \
    io/visualstudiosolutionwriter.cpp \
    msbuild/imsbuildgroup.cpp \
    msbuild/imsbuildnode.cpp \
    msbuild/imsbuildproperty.cpp \
    msbuild/items/msbuildclcompile.cpp \
    msbuild/items/msbuildclinclude.cpp \
    msbuild/items/msbuildfileitem.cpp \
    msbuild/items/msbuildfilter.cpp \
    msbuild/items/msbuildlink.cpp \
    msbuild/items/msbuildnone.cpp \
    msbuild/msbuildimport.cpp \
    msbuild/msbuildimportgroup.cpp \
    msbuild/msbuilditem.cpp \
    msbuild/msbuilditemdefinitiongroup.cpp \
    msbuild/msbuilditemgroup.cpp \
    msbuild/msbuilditemmetadata.cpp \
    msbuild/msbuildproject.cpp \
    msbuild/msbuildproperty.cpp \
    msbuild/msbuildpropertygroup.cpp \
    solution/ivisualstudiosolutionproject.cpp \
    solution/visualstudiosolution.cpp \
    solution/visualstudiosolutionfileproject.cpp \
    solution/visualstudiosolutionfolderproject.cpp \
    solution/visualstudiosolutionglobalsection.cpp
