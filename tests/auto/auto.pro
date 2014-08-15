TEMPLATE=subdirs

qbs_enable_unit_tests {
    SUBDIRS += \
        buildgraph \
        language \
        tools \
}

SUBDIRS += \
    cmdlineparser \
    blackbox \
    api
