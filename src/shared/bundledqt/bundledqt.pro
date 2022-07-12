TEMPLATE = aux

qbs_enable_bundled_qt {
    !isEmpty(QBS_APPS_DESTDIR): bindir = $${QBS_APPS_DESTDIR}
    else: bindir = bin

    !isEmpty(QBS_LIBRARY_DIRNAME): libdir = $${QBS_LIBRARY_DIRNAME}
    else: libdir = lib

    win32: dstdir = $${bindir}
    else: dstdir = $${libdir}
    dstdir=$$shell_quote($$shell_path($$absolute_path($$dstdir,  $${OUT_PWD}/../../..)))

    win32: bundled_qt.path = $${QBS_INSTALL_PREFIX}/$${bindir}
    else: bundled_qt.path = $${QBS_INSTALL_PREFIX}/$${libdir}

    target_qt_libs = Qt5Core Qt5Xml Qt5Network Qt5Script
    bundled_qt_at_build.target = bundled_qt_libs

    for(lib, target_qt_libs) {
        win32: srcdir = $$[QT_INSTALL_BINS]
        else: srcdir = $$[QT_INSTALL_LIBS]
        srcdir = $$shell_quote($$shell_path($${srcdir}))

        win32: file = "$${lib}.dll"
        macos: file = "lib$${lib}*.dylib"
        linux-*: file = "lib$${lib}.so*"
        file = $$shell_path($$absolute_path($${file}, $${srcdir}))

        win32:bundled_qt.files += $${file}
        else: bundled_qt.extra += $${QMAKE_COPY} -P $${file} $(INSTALL_ROOT)/$${bundled_qt.path} $$escape_expand(\n\t)

        !win32: copy_flags = -P
        bundled_qt_at_build.commands += $${QMAKE_COPY} $${copy_flags} $${file} $${dstdir} $$escape_expand(\n\t)
    }

    QMAKE_EXTRA_TARGETS += bundled_qt_at_build
    PRE_TARGETDEPS += $${bundled_qt_at_build.target}

    INSTALLS += bundled_qt
}
