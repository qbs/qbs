include(../qbs_version.pri)

qbsdoc_version.name = QBS_VERSION
qbsdoc_version.value = $$QBS_VERSION
qbsdoc_versiontag.name = QBS_VERSION_TAG
qbsdoc_versiontag.value = $$replace(QBS_VERSION, "[-.]", )
qbsdoc_qtdocs.name = QT_INSTALL_DOCS
qbsdoc_qtdocs.value = $$[QT_INSTALL_DOCS/src]
qbsdoc_builddir.name = BUILDDIR
qbsdoc_builddir.value = $$OUT_PWD
QDOC_ENV += qbsdoc_version qbsdoc_versiontag qbsdoc_qtdocs qbsdoc_builddir

build_online_docs: \
    DOC_FILES += $$PWD/qbs-online.qdocconf
else: \
    DOC_FILES += $$PWD/qbs.qdocconf
