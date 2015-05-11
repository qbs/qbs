include(../../../install_prefix.pri)

SOURCES += \
    $$PWD/abstractcommandexecutor.cpp \
    $$PWD/artifact.cpp \
    $$PWD/artifactcleaner.cpp \
    $$PWD/artifactset.cpp \
    $$PWD/artifactvisitor.cpp \
    $$PWD/buildgraph.cpp \
    $$PWD/buildgraphloader.cpp \
    $$PWD/buildgraphnode.cpp \
    $$PWD/command.cpp \
    $$PWD/cycledetector.cpp \
    $$PWD/depscanner.cpp \
    $$PWD/emptydirectoriesremover.cpp \
    $$PWD/executor.cpp \
    $$PWD/executorjob.cpp \
    $$PWD/filedependency.cpp \
    $$PWD/inputartifactscanner.cpp \
    $$PWD/jscommandexecutor.cpp \
    $$PWD/nodeset.cpp \
    $$PWD/nodetreedumper.cpp \
    $$PWD/processcommandexecutor.cpp \
    $$PWD/productbuilddata.cpp \
    $$PWD/productinstaller.cpp \
    $$PWD/projectbuilddata.cpp \
    $$PWD/qtmocscanner.cpp \
    $$PWD/rescuableartifactdata.cpp \
    $$PWD/rulegraph.cpp \
    $$PWD/rulenode.cpp \
    $$PWD/rulesapplicator.cpp \
    $$PWD/rulesevaluationcontext.cpp \
    $$PWD/scanresultcache.cpp \
    $$PWD/timestampsupdater.cpp \
    $$PWD/transformer.cpp

HEADERS += \
    $$PWD/abstractcommandexecutor.h \
    $$PWD/artifact.h \
    $$PWD/artifactcleaner.h \
    $$PWD/artifactset.h \
    $$PWD/artifactvisitor.h \
    $$PWD/buildgraph.h \
    $$PWD/buildgraphloader.h \
    $$PWD/buildgraphnode.h \
    $$PWD/buildgraphvisitor.h \
    $$PWD/command.h \
    $$PWD/cycledetector.h \
    $$PWD/depscanner.h \
    $$PWD/emptydirectoriesremover.h \
    $$PWD/executor.h \
    $$PWD/executorjob.h \
    $$PWD/filedependency.h \
    $$PWD/forward_decls.h \
    $$PWD/inputartifactscanner.h \
    $$PWD/jscommandexecutor.h \
    $$PWD/nodeset.h \
    $$PWD/nodetreedumper.h \
    $$PWD/processcommandexecutor.h \
    $$PWD/productbuilddata.h \
    $$PWD/productinstaller.h \
    $$PWD/projectbuilddata.h \
    $$PWD/qtmocscanner.h \
    $$PWD/rescuableartifactdata.h \
    $$PWD/rulegraph.h \
    $$PWD/rulenode.h \
    $$PWD/rulesapplicator.h \
    $$PWD/rulesevaluationcontext.h \
    $$PWD/scanresultcache.h \
    $$PWD/timestampsupdater.h \
    $$PWD/transformer.h

qbs_enable_unit_tests {
    HEADERS += $$PWD/tst_buildgraph.h
    SOURCES += $$PWD/tst_buildgraph.cpp
}

!qbs_no_dev_install {
    buildgraph_headers.files = $$PWD/forward_decls.h
    buildgraph_headers.path = $${QBS_INSTALL_PREFIX}/include/qbs/buildgraph
    INSTALLS += buildgraph_headers
}
