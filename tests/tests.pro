TEMPLATE = subdirs
SUBDIRS = auto fuzzy-test

qtHaveModule(concurrent): SUBDIRS += benchmarker
