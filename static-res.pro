TEMPLATE = aux

!isEmpty(QBS_APPS_DESTDIR): qbsbindir = $${QBS_APPS_DESTDIR}
else: qbsbindir = bin

builddirname = qbsres
typedescdir = share/qbs/qml-type-descriptions
typedescdir_src = $$builddirname/default/install-root/$$typedescdir
!isEmpty(QBS_QML_TYPE_DESCRIPTIONS_BUILD_DIR): \
    typedescdir_dst = $$QBS_QML_TYPE_DESCRIPTIONS_BUILD_DIR
else:!isEmpty(QBS_RESOURCES_BUILD_DIR): \
    typedescdir_dst = $$QBS_RESOURCES_BUILD_DIR/$$typedescdir
else: \
    typedescdir_dst = $$typedescdir

qbsres.target = $$builddirname/default/default.bg
qbsres.commands = \
    $$shell_quote($$shell_path($$qbsbindir/qbs)) \
    build \
    --settings-dir $$shell_quote($$builddirname/settings) \
    -f $$shell_quote($$PWD/qbs.qbs) \
    -d $$shell_quote($$builddirname) \
    -p $$shell_quote("qbs resources") \
    project.withCode:false \
    project.withDocumentation:false \
    profile:none

qbsqmltypes.target = $$typedescdir_dst/qbs.qmltypes
qbsqmltypes.commands = \
    $$sprintf($$QMAKE_MKDIR_CMD, \
        $$shell_quote($$shell_path($$typedescdir_dst))) $$escape_expand(\\n\\t) \
    $$QMAKE_COPY \
        $$shell_quote($$shell_path($$typedescdir_src/qbs.qmltypes)) \
        $$shell_quote($$shell_path($$typedescdir_dst/qbs.qmltypes))
qbsqmltypes.depends += qbsres

qbsbundle.target = $$typedescdir_dst/qbs-bundle.json
qbsbundle.commands = \
    $$sprintf($$QMAKE_MKDIR_CMD, \
       $$shell_quote($$shell_path($$typedescdir_dst))) $$escape_expand(\\n\\t) \
    $$QMAKE_COPY \
        $$shell_quote($$shell_path($$typedescdir_src/qbs-bundle.json)) \
        $$shell_quote($$shell_path($$typedescdir_dst/qbs-bundle.json))
qbsbundle.depends += qbsres

QMAKE_EXTRA_TARGETS += qbsres qbsqmltypes qbsbundle

PRE_TARGETDEPS += $$qbsqmltypes.target $$qbsbundle.target

include(src/install_prefix.pri)

qbstypedescfiles.files = $$qbsqmltypes.target $$qbsbundle.target
!isEmpty(QBS_QML_TYPE_DESCRIPTIONS_INSTALL_DIR): \
    installPrefix = $${QBS_QML_TYPE_DESCRIPTIONS_INSTALL_DIR}
else:!isEmpty(QBS_RESOURCES_INSTALL_DIR): \
    installPrefix = $${QBS_RESOURCES_INSTALL_DIR}/$$typedescdir
else: \
    installPrefix = $${QBS_INSTALL_PREFIX}/$$typedescdir
qbstypedescfiles.path = $${installPrefix}
INSTALLS += qbstypedescfiles
