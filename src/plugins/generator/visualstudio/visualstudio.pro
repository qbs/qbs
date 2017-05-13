include(../../plugins.pri)

TARGET = visualstudiogenerator

QT = core

HEADERS += \
    $$PWD/msbuildfiltersproject.h \
    $$PWD/msbuildqbsgenerateproject.h \
    $$PWD/msbuildqbsproductproject.h \
    $$PWD/msbuildsharedsolutionpropertiesproject.h \
    $$PWD/msbuildsolutionpropertiesproject.h \
    $$PWD/msbuildtargetproject.h \
    $$PWD/msbuildutils.h \
    $$PWD/visualstudiogenerator.h \
    $$PWD/visualstudioguidpool.h

SOURCES += \
    $$PWD/msbuildfiltersproject.cpp \
    $$PWD/msbuildqbsgenerateproject.cpp \
    $$PWD/msbuildqbsproductproject.cpp \
    $$PWD/msbuildsharedsolutionpropertiesproject.cpp \
    $$PWD/msbuildsolutionpropertiesproject.cpp \
    $$PWD/msbuildtargetproject.cpp \
    $$PWD/visualstudiogenerator.cpp \
    $$PWD/visualstudiogeneratorplugin.cpp \
    $$PWD/visualstudioguidpool.cpp

HEADERS += \
    $$PWD/solution/ivisualstudiosolutionproject.h \
    $$PWD/solution/visualstudiosolutionfileproject.h \
    $$PWD/solution/visualstudiosolutionfolderproject.h \
    $$PWD/solution/visualstudiosolution.h \
    $$PWD/solution/visualstudiosolutionglobalsection.h \

SOURCES += \
    $$PWD/solution/ivisualstudiosolutionproject.cpp \
    $$PWD/solution/visualstudiosolutionfileproject.cpp \
    $$PWD/solution/visualstudiosolutionfolderproject.cpp \
    $$PWD/solution/visualstudiosolution.cpp \
    $$PWD/solution/visualstudiosolutionglobalsection.cpp

HEADERS += \
    $$PWD/msbuild/imsbuildgroup.h \
    $$PWD/msbuild/imsbuildnode.h \
    $$PWD/msbuild/imsbuildnodevisitor.h \
    $$PWD/msbuild/imsbuildproperty.h \
    $$PWD/msbuild/msbuildimport.h \
    $$PWD/msbuild/msbuildimportgroup.h \
    $$PWD/msbuild/msbuilditem.h \
    $$PWD/msbuild/msbuilditemdefinitiongroup.h \
    $$PWD/msbuild/msbuilditemgroup.h \
    $$PWD/msbuild/msbuilditemmetadata.h \
    $$PWD/msbuild/msbuildproject.h \
    $$PWD/msbuild/msbuildproperty.h \
    $$PWD/msbuild/msbuildpropertygroup.h

SOURCES += \
    $$PWD/msbuild/imsbuildgroup.cpp \
    $$PWD/msbuild/imsbuildnode.cpp \
    $$PWD/msbuild/imsbuildproperty.cpp \
    $$PWD/msbuild/msbuildimport.cpp \
    $$PWD/msbuild/msbuildimportgroup.cpp \
    $$PWD/msbuild/msbuilditem.cpp \
    $$PWD/msbuild/msbuilditemdefinitiongroup.cpp \
    $$PWD/msbuild/msbuilditemgroup.cpp \
    $$PWD/msbuild/msbuilditemmetadata.cpp \
    $$PWD/msbuild/msbuildproject.cpp \
    $$PWD/msbuild/msbuildproperty.cpp \
    $$PWD/msbuild/msbuildpropertygroup.cpp

HEADERS += \
    $$PWD/msbuild/items/msbuildclcompile.h \
    $$PWD/msbuild/items/msbuildclinclude.h \
    $$PWD/msbuild/items/msbuildfileitem.h \
    $$PWD/msbuild/items/msbuildfilter.h \
    $$PWD/msbuild/items/msbuildlink.h \
    $$PWD/msbuild/items/msbuildnone.h

SOURCES += \
    $$PWD/msbuild/items/msbuildclcompile.cpp \
    $$PWD/msbuild/items/msbuildclinclude.cpp \
    $$PWD/msbuild/items/msbuildfileitem.cpp \
    $$PWD/msbuild/items/msbuildfilter.cpp \
    $$PWD/msbuild/items/msbuildlink.cpp \
    $$PWD/msbuild/items/msbuildnone.cpp

HEADERS += \
    $$PWD/io/msbuildprojectwriter.h \
    $$PWD/io/visualstudiosolutionwriter.h

SOURCES += \
    $$PWD/io/msbuildprojectwriter.cpp \
    $$PWD/io/visualstudiosolutionwriter.cpp
