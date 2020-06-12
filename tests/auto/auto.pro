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
    blackbox/blackbox-android.pro \
    blackbox/blackbox-apple.pro \
    blackbox/blackbox-baremetal.pro \
    blackbox/blackbox-clangdb.pro \
    blackbox/blackbox-examples.pro \
    blackbox/blackbox-java.pro \
    blackbox/blackbox-joblimits.pro \
    blackbox/blackbox-qt.pro \
    api
