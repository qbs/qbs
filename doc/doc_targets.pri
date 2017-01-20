# This pri file can be included by other projects that want to provide the qbs documentation.
# Common variables like QDOC_BIN or HELPGENERATOR must be provided by the including file.
QBS_VERSION_TAG = $$replace(QBS_VERSION, "[-.]", )

isEmpty(QBS_DOCS_BUILD_DIR): QBS_DOCS_BUILD_DIR = $$OUT_PWD/doc
isEmpty(QBS_HTML_DOC_PATH): QBS_HTML_DOC_PATH = $$QBS_DOCS_BUILD_DIR/html
equals(QMAKE_DIR_SEP, /) {   # unix, mingw+msys
    QDOC = SRCDIR=$$PWD OUTDIR=$$QBS_HTML_DOC_PATH QBS_VERSION=$$QBS_VERSION QBS_VERSION_TAG=$$QBS_VERSION_TAG QT_INSTALL_DOCS=$$[QT_INSTALL_DOCS] $$QDOC_BIN
} else: mingw {
    # The lack of spaces in front of the && is necessary!
    QDOC = set SRCDIR=$$PWD&& set OUTDIR=$$QBS_HTML_DOC_PATH&& set QBS_VERSION=$$QBS_VERSION&& set QBS_VERSION_TAG=$$QBS_VERSION_TAG&& set QT_INSTALL_DOCS=$$[QT_INSTALL_DOCS]&& $$QDOC_BIN
} else {   # nmake
    QDOC = set SRCDIR=$$PWD $$escape_expand(\\n\\t) \
           set OUTDIR=$$QBS_HTML_DOC_PATH $$escape_expand(\\n\\t) \
           set QBS_VERSION=$$QBS_VERSION $$escape_expand(\\n\\t) \
           set QBS_VERSION_TAG=$$QBS_VERSION_TAG $$escape_expand(\\n\\t) \
           set QT_INSTALL_DOCS=$$[QT_INSTALL_DOCS] $$escape_expand(\\n\\t) \
           $$QDOC_BIN
}

QBS_QHP_FILE = $$QBS_HTML_DOC_PATH/qbs.qhp
QBS_QCH_FILE = $$QBS_DOCS_BUILD_DIR/qbs.qch

QBS_HELP_DEP_FILES = \
    $$PWD/qbs.qdoc \
    $$PWD/qbs.qdocconf

qbs_html_docs.commands = $$QDOC $$PWD/qbs.qdocconf
qbs_html_docs.depends += $$QBS_HELP_DEP_FILES

qbs_qch_docs.commands = $$HELPGENERATOR -o $$shell_quote($$QBS_QCH_FILE) $$QBS_QHP_FILE
qbs_qch_docs.depends += qbs_html_docs

isEmpty(QBS_DOCS_INSTALL_DIR): QBS_DOCS_INSTALL_DIR = $${QBS_INSTALL_PREFIX}/share/doc/qbs
inst_qbs_qch_docs.files = $$QBS_QCH_FILE
inst_qbs_qch_docs.path = $$QBS_DOCS_INSTALL_DIR
inst_qbs_qch_docs.CONFIG += no_check_exist no_default_install
INSTALLS += inst_qbs_qch_docs

inst_qbs_html_docs.files = $$QBS_HTML_DOC_PATH
inst_qbs_html_docs.path = $$inst_qbs_qch_docs.path
inst_qbs_html_docs.CONFIG += no_check_exist no_default_install directory
INSTALLS += inst_qbs_html_docs

qbs_install_docs.depends = install_inst_qbs_qch_docs install_inst_qbs_html_docs
QMAKE_EXTRA_TARGETS += qbs_install_docs

qbs_docs.depends = qbs_qch_docs
QMAKE_EXTRA_TARGETS += qbs_html_docs qbs_qch_docs qbs_docs
