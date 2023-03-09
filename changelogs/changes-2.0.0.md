# General
* Switched JavaScript engine from QtScript to QuickJS
* Removed the long-deprecated loadFile() and loadExtension() functions
* Removed the qmake project files

# Qt support
* Adapt to androiddeployqt on Windows no longer accepting tool paths without suffix in 6.4 (QTBUG-111558)

# BareMetal support
* Added support for HPPA architectures

# Other modules
* Renamed "name" to "appName" in the freedesktop module to prevent clash with built-in property

# Infrastructure
* Added USBSAN CI job

# Contributors
* Christian Kandeler
* Ivan Komissarov
* Orgad Shaneh
* Pino Toscano
