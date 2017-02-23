include(../../../install_prefix.pri)

HEADERS += \
    $$PWD/artifactproperties.h \
    $$PWD/astimportshandler.h \
    $$PWD/astpropertiesitemhandler.h \
    $$PWD/asttools.h \
    $$PWD/builtindeclarations.h \
    $$PWD/deprecationinfo.h \
    $$PWD/evaluationdata.h \
    $$PWD/evaluator.h \
    $$PWD/evaluatorscriptclass.h \
    $$PWD/filecontext.h \
    $$PWD/filecontextbase.h \
    $$PWD/filetags.h \
    $$PWD/forward_decls.h \
    $$PWD/identifiersearch.h \
    $$PWD/item.h \
    $$PWD/itemdeclaration.h \
    $$PWD/itemobserver.h \
    $$PWD/itempool.h \
    $$PWD/itemreader.h \
    $$PWD/itemreaderastvisitor.h \
    $$PWD/itemreadervisitorstate.h \
    $$PWD/itemtype.h \
    $$PWD/jsimports.h \
    $$PWD/language.h \
    $$PWD/loader.h \
    $$PWD/moduleloader.h \
    $$PWD/modulemerger.h \
    $$PWD/moduleproviderinfo.h \
    $$PWD/preparescriptobserver.h \
    $$PWD/projectresolver.h \
    $$PWD/property.h \
    $$PWD/propertydeclaration.h \
    $$PWD/propertymapinternal.h \
    $$PWD/qualifiedid.h \
    $$PWD/resolvedfilecontext.h \
    $$PWD/scriptengine.h \
    $$PWD/scriptimporter.h \
    $$PWD/scriptpropertyobserver.h \
    $$PWD/value.h

SOURCES += \
    $$PWD/artifactproperties.cpp \
    $$PWD/astimportshandler.cpp \
    $$PWD/astpropertiesitemhandler.cpp \
    $$PWD/asttools.cpp \
    $$PWD/builtindeclarations.cpp \
    $$PWD/evaluator.cpp \
    $$PWD/evaluatorscriptclass.cpp \
    $$PWD/filecontext.cpp \
    $$PWD/filecontextbase.cpp \
    $$PWD/filetags.cpp \
    $$PWD/identifiersearch.cpp \
    $$PWD/item.cpp \
    $$PWD/itemdeclaration.cpp \
    $$PWD/itempool.cpp \
    $$PWD/itemreader.cpp \
    $$PWD/itemreaderastvisitor.cpp \
    $$PWD/itemreadervisitorstate.cpp \
    $$PWD/language.cpp \
    $$PWD/loader.cpp \
    $$PWD/moduleloader.cpp \
    $$PWD/modulemerger.cpp \
    $$PWD/preparescriptobserver.cpp \
    $$PWD/scriptpropertyobserver.cpp \
    $$PWD/projectresolver.cpp \
    $$PWD/property.cpp \
    $$PWD/propertydeclaration.cpp \
    $$PWD/propertymapinternal.cpp \
    $$PWD/qualifiedid.cpp \
    $$PWD/resolvedfilecontext.cpp \
    $$PWD/scriptengine.cpp \
    $$PWD/scriptimporter.cpp \
    $$PWD/value.cpp

!qbs_no_dev_install {
    language_headers.files = $$PWD/forward_decls.h
    language_headers.path = $${QBS_INSTALL_PREFIX}/include/qbs/language
    INSTALLS += language_headers
}
