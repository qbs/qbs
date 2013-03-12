QT = core
TEMPLATE = app
DESTDIR = ../../../bin

CONFIG += console
CONFIG -= app_bundle

include($${PWD}/../lib/use.pri)
include($${PWD}/shared/logging/logging.pri)

target.path = $${QBS_INSTALL_PREFIX}/bin
INSTALLS += target
