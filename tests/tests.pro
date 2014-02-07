TEMPLATE = subdirs
SUBDIRS = auto
greaterThan(QT_MAJOR_VERSION, 4):SUBDIRS += fuzzy-test # We use QDir::removeRecursively()
