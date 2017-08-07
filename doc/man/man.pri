qbs_no_man_install: return()

man.files = $$PWD/qbs.1
man.path = $${QBS_INSTALL_PREFIX}/share/man/man1
INSTALLS += man
