set(SOURCES
    commandlineparser.cpp
    commandlineparser.h
    fuzzytester.cpp
    fuzzytester.h
    main.cpp
    )

add_qbs_app(qbs_fuzzy-test
    DEPENDS Qt${QT_VERSION_MAJOR}::Core
    SOURCES ${SOURCES}
    )
