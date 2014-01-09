QT = core
TEMPLATE = app
DESTDIR = ../../../bin

CONFIG += console
CONFIG -= app_bundle

include($${PWD}/../lib/corelib/use_corelib.pri)
include($${PWD}/shared/logging/logging.pri)

target.path = $${QBS_INSTALL_PREFIX}/bin
INSTALLS += target
