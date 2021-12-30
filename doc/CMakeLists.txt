set(_DOC_SOURCES
    ../README.md
    ../CONTRIBUTING.md
    classic.css
    external-resources.qdoc
    fixnavi.pl
    howtos.qdoc
    qbs.qdoc
    qbs-online.qdocconf
    config/style/qt5-sidebar.html
    )

file(GLOB_RECURSE _DOC_APPENDIX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/appendix/*")
file(GLOB_RECURSE _DOC_REFERENCE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/reference/*")
file(GLOB_RECURSE _DOC_TEMPLATES_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/templates/*")
file(GLOB_RECURSE _DOC_IMAGES_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/images/*")
file(GLOB_RECURSE _DOC_TARGETS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/targets/*")

add_qbs_documentation(
    "qbs.qdocconf"
    SOURCES
        ${_DOC_SOURCES}
        ${_DOC_APPENDIX_SOURCES}
        ${_DOC_REFERENCE_SOURCES}
        ${_DOC_TEMPLATES_SOURCES}
        ${_DOC_IMAGES_SOURCES}
        ${_DOC_TARGETS_SOURCES}
    )

add_subdirectory(man)
