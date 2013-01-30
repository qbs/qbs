TEMPLATE = subdirs
SUBDIRS =\
    qbs\
    detect-toolchains \
    setupmaddeplatforms \
    qbs-setup-qt \
    config \
    qbs-qmltypes

lessThan(QT_MAJOR_VERSION, 5)|!isEmpty(QT.widgets.name):SUBDIRS += config-ui
