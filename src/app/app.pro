TEMPLATE = subdirs
SUBDIRS =\
    qbs\
    qbs-create-project \
    qbs-setup-android \
    qbs-setup-toolchains \
    qbs-setup-qt \
    config \
    ../shared/bundledqt

!isEmpty(QT.widgets.name):SUBDIRS += config-ui
