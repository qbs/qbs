set(SOURCES
    clangclprobe.cpp
    clangclprobe.h
    commandlineparser.cpp
    commandlineparser.h
    cosmicprobe.cpp
    cosmicprobe.h
    dmcprobe.cpp
    dmcprobe.h
    gccprobe.cpp
    gccprobe.h
    iarewprobe.cpp
    iarewprobe.h
    keilprobe.cpp
    keilprobe.h
    main.cpp
    msvcprobe.cpp
    msvcprobe.h
    probe.cpp
    probe.h
    sdccprobe.cpp
    sdccprobe.h
    watcomprobe.cpp
    watcomprobe.h
    xcodeprobe.cpp
    xcodeprobe.h
    )

add_qbs_app(qbs-setup-toolchains
    DEPENDS qbscore qbsconsolelogger
    SOURCES ${SOURCES}
    )
