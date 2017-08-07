include(../src/install_prefix.pri)

include(doc_shared.pri)

DOC_OUTDIR_POSTFIX = /html
DOC_HTML_INSTALLDIR = $$QBS_INSTALL_PREFIX/share/doc/qbs
DOC_QCH_OUTDIR = $$OUT_PWD/doc
DOC_QCH_INSTALLDIR = $$QBS_INSTALL_PREFIX/share/doc/qbs

include(doc_targets.pri)

fixnavi.commands = \
    cd $$shell_path($$PWD) && \
    perl fixnavi.pl -Dqcmanual -Dqtquick \
        qbs.qdoc
QMAKE_EXTRA_TARGETS += fixnavi

include(man/man.pri)
