set(SOURCES
    activities.h
    benchmarker-main.cpp
    benchmarker.cpp
    benchmarker.h
    commandlineparser.cpp
    commandlineparser.h
    exception.h
    runsupport.cpp
    runsupport.h
    valgrindrunner.cpp
    valgrindrunner.h
    )

add_qbs_app(qbs_benchmarker
    DEPENDS Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Concurrent
    SOURCES ${SOURCES}
    )
