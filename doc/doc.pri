defineReplace(targetPath) {
    return($$replace(1, /, $$QMAKE_DIR_SEP))
}

qt:greaterThan(QT_MAJOR_VERSION, 4) {
    QDOC_BIN = $$targetPath($$[QT_INSTALL_BINS]/qdoc)
    QDOC_MAINFILE = $$PWD/qbs.qdocconf
} else {
    QDOC_BIN = $$targetPath($$[QT_INSTALL_BINS]/qdoc3)
    QDOC_MAINFILE = $$PWD/qbs-qt4.qdocconf

    # cheap replacement of the Qt5 shell_quote function
    defineReplace(shell_quote) {
        return("\"$$1\"")
    }
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
                 $$PWD/config/compat-qt4.qdocconf \
                 $$PWD/config/macros.qdocconf \
                 $$PWD/config/qt-cpp-ignore.qdocconf \
                 $$PWD/config/qt-html-templates.qdocconf \
                 $$PWD/config/qt-html-default-styles.qdocconf \
                 $$QDOC_MAINFILE

html_docs.commands = $$QDOC $$QDOC_MAINFILE
html_docs.depends += $$HELP_DEP_FILES
html_docs.files = $$QHP_FILE

qch_docs.commands = $$HELPGENERATOR -o $$shell_quote($$QCH_FILE) $$QHP_FILE
qch_docs.depends += html_docs
qch_docs.files = $$QCH_FILE

qch_docs.path = $${QBS_INSTALL_PREFIX}/share/doc/qbs
qch_docs.CONFIG += no_check_exist
INSTALLS += qch_docs

docs.depends = qch_docs
QMAKE_EXTRA_TARGETS += html_docs qch_docs docs

fixnavi.commands = \
    cd $$targetPath($$PWD) && \
    perl fixnavi.pl -Dqcmanual -Dqtquick \
        qbs.qdoc
QMAKE_EXTRA_TARGETS += fixnavi
