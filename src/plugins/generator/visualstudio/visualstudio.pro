include(visualstudio.pri)
include(../../plugins.pri)
include(../../../shared/json/json.pri)
include(../../../lib/msbuild/use_msbuild.pri)
# Using the indirect usage of corelib via plugins.pri breaks linking on mingw
include(../../../lib/corelib/use_corelib.pri)

INCLUDEPATH += ../../../lib/msbuild

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
