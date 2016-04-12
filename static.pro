TEMPLATE = aux

DATA_DIRS = share/qbs/imports share/qbs/modules
win32:DATA_FILES = $$PWD/bin/ibmsvc.xml $$PWD/bin/ibqbs.bat

# For use in custom compilers which just copy files
defineReplace(stripSrcDir) {
    return($$relative_path($$absolute_path($$1, $$OUT_PWD), $$_PRO_FILE_PWD_))
}

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    for(file, files):!exists($$file/*):FILES += $$file
}
FILES += $$DATA_FILES

OTHER_FILES += $$FILES

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

include(src/install_prefix.pri)

share.files = share/qbs
!isEmpty(QBS_RESOURCES_INSTALL_DIR): \
    installPrefix = $${QBS_RESOURCES_INSTALL_DIR}
else: \
    installPrefix = $${QBS_INSTALL_PREFIX}
share.path = $${installPrefix}/share
examples.files = examples
examples.path = $${share.path}/qbs
INSTALLS += share examples
