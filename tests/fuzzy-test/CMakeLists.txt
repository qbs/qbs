set(SOURCES
    commandlineparser.cpp
    commandlineparser.h
    fuzzytester.cpp
    fuzzytester.h
    main.cpp
    )

add_qbs_app(qbs_fuzzy-test
    DEPENDS Qt5::Core
    SOURCES ${SOURCES}
    )
