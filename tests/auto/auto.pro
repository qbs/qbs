TEMPLATE=subdirs

all_tests {
    SUBDIRS += \
        buildgraph \
        language \
        tools \
}

SUBDIRS += \
    cmdlineparser \
    blackbox \
    api
