include(../install_prefix.pri)

win32:LIBEXEC_BASE_DIR=bin
else:LIBEXEC_BASE_DIR=libexec/qbs

!isEmpty(QBS_LIBEXEC_DESTDIR):DESTDIR=$${QBS_LIBEXEC_DESTDIR}
else:DESTDIR=../../../$$LIBEXEC_BASE_DIR

!isEmpty(QBS_LIBEXEC_INSTALL_DIR):target.path = $${QBS_LIBEXEC_INSTALL_DIR}
else:target.path = $${QBS_INSTALL_PREFIX}/$$LIBEXEC_BASE_DIR
INSTALLS += target
