TEMPLATE=subdirs

qbs_enable_unit_tests {
    SUBDIRS += \
        buildgraph \
        language \
        tools \
}

SUBDIRS += \
    cmdlineparser \
    blackbox/blackbox.pro \
    blackbox/blackbox-clangdb.pro \
    blackbox/blackbox-java.pro \
    api
