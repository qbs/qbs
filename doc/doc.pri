defineReplace(targetPath) {
    return($$replace(1, /, $$QMAKE_DIR_SEP))
}

qt:greaterThan(QT_MAJOR_VERSION, 4) {
    QDOC_BIN = $$targetPath($$[QT_INSTALL_BINS]/qdoc)
} else {
    QDOC_BIN = $$targetPath($$[QT_INSTALL_BINS]/qdoc3)
}

HELPGENERATOR = $$targetPath($$[QT_INSTALL_BINS]/qhelpgenerator)

VERSION_TAG = $$replace(QBS_VERSION, "[-.]", )

equals(QMAKE_DIR_SEP, /) {   # unix, mingw+msys
    QDOC = SRCDIR=$$PWD OUTDIR=$$OUT_PWD/doc/html QBS_VERSION=$$QBS_VERSION QBS_VERSION_TAG=$$VERSION_TAG $$QDOC_BIN
} else:win32-g++* {   # just mingw
    # The lack of spaces in front of the && is necessary!
    QDOC = set SRCDIR=$$PWD&& set OUTDIR=$$OUT_PWD/doc/html&& set QBS_VERSION=$$QBS_VERSION&& set QBS_VERSION_TAG=$$VERSION_TAG&& $$QDOC_BIN
} else {   # nmake
    QDOC = set SRCDIR=$$PWD $$escape_expand(\\n\\t) \
           set OUTDIR=$$OUT_PWD/doc/html $$escape_expand(\\n\\t) \
           set QBS_VERSION=$$QBS_VERSION $$escape_expand(\\n\\t) \
           set QBS_VERSION_TAG=$$VERSION_TAG $$escape_expand(\\n\\t) \
           $$QDOC_BIN
}

QHP_FILE = $$OUT_PWD/doc/html/qbs.qhp
QCH_FILE = $$OUT_PWD/doc/qbs.qch

HELP_DEP_FILES = $$PWD/qbs.qdoc \
                 $$PWD/config/compat.qdocconf \
                 $$PWD/config/macros.qdocconf \
                 $$PWD/config/qt-cpp-ignore.qdocconf \
                 $$PWD/config/qt-html-templates.qdocconf \
                 $$PWD/config/qt-html-default-styles.qdocconf \
                 $$PWD/qbs.qdocconf

html_docs.commands = $$QDOC $$PWD/qbs.qdocconf
html_docs.depends += $$HELP_DEP_FILES
html_docs.files = $$QHP_FILE

html_docs_online.commands = $$QDOC $$PWD/qbs-online.qdocconf
html_docs_online.depends += $$HELP_DEP_FILES

qch_docs.commands = $$HELPGENERATOR -o \"$$QCH_FILE\" $$QHP_FILE
qch_docs.depends += html_docs
qch_docs.files = $$QCH_FILE

unix:!macx {
    qch_docs.path = /share/doc/qbs
    qch_docs.CONFIG += no_check_exist
    INSTALLS += qch_docs
}

docs_online.depends = html_docs_online
docs.depends = qch_docs
QMAKE_EXTRA_TARGETS += html_docs html_docs_online qch_docs docs docs_online

fixnavi.commands = \
    cd $$targetPath($$PWD) && \
    perl fixnavi.pl -Dqcmanual -Dqtquick \
        qbs.qdoc
QMAKE_EXTRA_TARGETS += fixnavi
