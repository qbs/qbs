SOURCES += \
    $$PWD/automoc.cpp\
    $$PWD/buildgraph.cpp\
    $$PWD/executor.cpp\
    $$PWD/executorjob.cpp\
    $$PWD/rulegraph.cpp\
    $$PWD/scanresultcache.cpp \
    $$PWD/artifactlist.cpp \
    $$PWD/command.cpp \
    $$PWD/abstractcommandexecutor.cpp \
    $$PWD/processcommandexecutor.cpp \
    $$PWD/jscommandexecutor.cpp \
    $$PWD/transformer.cpp \
    $$PWD/artifact.cpp \
    $$PWD/inputartifactscanner.cpp \
    $$PWD/artifactvisitor.cpp \
    $$PWD/artifactcleaner.cpp \
    $$PWD/productinstaller.cpp \
    $$PWD/cycledetector.cpp \
    $$PWD/rulesevaluationcontext.cpp \
    $$PWD/buildproduct.cpp \
    $$PWD/buildproject.cpp \
    $$PWD/rulesapplicator.cpp \
    $$PWD/timestampsupdater.cpp

HEADERS += \
    $$PWD/automoc.h\
    $$PWD/buildgraph.h\
    $$PWD/executor.h\
    $$PWD/executorjob.h\
    $$PWD/rulegraph.h\
    $$PWD/scanresultcache.h \
    $$PWD/artifactlist.h \
    $$PWD/command.h \
    $$PWD/abstractcommandexecutor.h \
    $$PWD/processcommandexecutor.h \
    $$PWD/jscommandexecutor.h \
    $$PWD/transformer.h \
    $$PWD/artifact.h \
    $$PWD/inputartifactscanner.h \
    $$PWD/artifactvisitor.h \
    $$PWD/artifactcleaner.h \
    $$PWD/productinstaller.h \
    $$PWD/cycledetector.h \
    $$PWD/forward_decls.h \
    $$PWD/rulesevaluationcontext.h \
    $$PWD/buildproduct.h \
    $$PWD/buildproject.h \
    $$PWD/rulesapplicator.h \
    $$PWD/timestampsupdater.h

all_tests {
    HEADERS += $$PWD/tst_buildgraph.h
    SOURCES += $$PWD/tst_buildgraph.cpp
}
