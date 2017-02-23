TEMPLATE = aux

DATA_DIRS = share/qbs/imports share/qbs/modules share/qbs/module-providers
PYTHON_DATA_DIRS = src/3rdparty/python/lib
win32:DATA_FILES = $$PWD/bin/ibmsvc.xml $$PWD/bin/ibqbs.bat
LIBEXEC_FILES = $$PWD/src/3rdparty/python/bin/dmgbuild

# For use in custom compilers which just copy files
defineReplace(stripSrcDir) {
    return($$relative_path($$absolute_path($$1, $$OUT_PWD), $$_PRO_FILE_PWD_))
}

defineReplace(stripPythonSrcDir) {
    return($$relative_path($$absolute_path($$1, $$OUT_PWD), \
        $$_PRO_FILE_PWD_/src/3rdparty/python/lib/python2.7/site-packages))
}

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    for(file, files):!exists($$file/*):FILES += $$file
}
FILES += $$DATA_FILES

for(data_dir, PYTHON_DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    for(file, files):!exists($$file/*):PYTHON_FILES += $$file
}
PYTHON_FILES += $$PYTHON_DATA_FILES

OTHER_FILES += $$FILES $$LIBEXEC_FILES

!isEqual(PWD, $$OUT_PWD)|!isEmpty(QBS_RESOURCES_BUILD_DIR) {
    copy2build.input = FILES
    !isEmpty(QBS_RESOURCES_BUILD_DIR): \
        copy2build.output = $${QBS_RESOURCES_BUILD_DIR}/${QMAKE_FUNC_FILE_IN_stripSrcDir}
    else: \
        copy2build.output = ${QMAKE_FUNC_FILE_IN_stripSrcDir}
    copy2build.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
    copy2build.name = COPY ${QMAKE_FILE_IN}
    copy2build.CONFIG += no_link target_predeps
    QMAKE_EXTRA_COMPILERS += copy2build
}

copy2build_python.input = PYTHON_FILES
!isEmpty(QBS_RESOURCES_BUILD_DIR): \
    copy2build_python.output = \
        $${QBS_RESOURCES_BUILD_DIR}/share/qbs/python/${QMAKE_FUNC_FILE_IN_stripPythonSrcDir}
else: \
    copy2build_python.output = share/qbs/python/${QMAKE_FUNC_FILE_IN_stripPythonSrcDir}
copy2build_python.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
copy2build_python.name = COPY ${QMAKE_FILE_IN}
copy2build_python.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += copy2build_python

libexec_copy.input = LIBEXEC_FILES
!isEmpty(QBS_LIBEXEC_DESTDIR): \
    libexec_copy.output = $${QBS_LIBEXEC_DESTDIR}/${QMAKE_FILE_IN_BASE}${QMAKE_FILE_EXT}
else: \
    libexec_copy.output = libexec/qbs/${QMAKE_FILE_IN_BASE}${QMAKE_FILE_EXT}
libexec_copy.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
libexec_copy.name = COPY ${QMAKE_FILE_IN}
libexec_copy.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += libexec_copy

include(src/install_prefix.pri)

share.files = share/qbs
!isEmpty(QBS_RESOURCES_INSTALL_DIR): \
    installPrefix = $${QBS_RESOURCES_INSTALL_DIR}
else: \
    installPrefix = $${QBS_INSTALL_PREFIX}
share.path = $${installPrefix}/share
examples.files = examples
examples.path = $${share.path}/qbs
python_bin.files = $$files(src/3rdparty/python/bin/*)
!isEmpty(QBS_LIBEXEC_INSTALL_DIR): \
    python_bin.path = $${QBS_LIBEXEC_INSTALL_DIR}
else: \
    python_bin.path = $${QBS_INSTALL_PREFIX}/libexec/qbs
python.files = $$files(src/3rdparty/python/lib/python2.7/site-packages/*.py, true)
python.base = $$PWD/src/3rdparty/python/lib/python2.7/site-packages
python.path = $${share.path}/qbs/python
INSTALLS += share examples python_bin python
