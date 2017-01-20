include(../src/install_prefix.pri)

defineReplace(targetPath) {
    return($$replace(1, /, $$QMAKE_DIR_SEP))
}

QDOC_BIN = $$targetPath($$[QT_INSTALL_BINS]/qdoc)
QDOC_MAINFILE = $$PWD/qbs.qdocconf
HELPGENERATOR = $$targetPath($$[QT_INSTALL_BINS]/qhelpgenerator)

include(doc_targets.pri)

html_docs.depends = qbs_html_docs
html_docs_online.depends = qbs_html_docs_online
qch_docs.depends = qbs_qch_docs
docs_online.depends = qbs_docs_online
install_docs.depends = qbs_install_docs
docs.depends = qbs_docs
QMAKE_EXTRA_TARGETS += \
    docs \
    docs_online \
    html_docs \
    html_docs_online \
    install_docs \
    qch_docs

fixnavi.commands = \
    cd $$targetPath($$PWD) && \
    perl fixnavi.pl -Dqcmanual -Dqtquick \
        qbs.qdoc
QMAKE_EXTRA_TARGETS += fixnavi
