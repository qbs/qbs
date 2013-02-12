TEMPLATE=subdirs
CONFIG += ordered

all_tests {
    SUBDIRS += \
        buildgraph \
        language \
        tools \
}

SUBDIRS += \
    cmdlineparser \
    blackbox
