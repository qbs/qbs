add_qbs_test(blackbox
    DEFINES
        ${QBS_UNIT_TESTS_DEFINES}
    SOURCES
        ../shared.h
        tst_blackboxbase.cpp
        tst_blackboxbase.h
        tst_blackbox.cpp
        tst_blackbox.h
    )

add_qbs_test(blackbox-android
    SOURCES
        ../shared.h
        tst_blackboxbase.cpp
        tst_blackboxbase.h
        tst_blackboxandroid.cpp
        tst_blackboxandroid.h
    )

add_qbs_test(blackbox-apple
    SOURCES
        ../shared.h
        tst_blackboxbase.cpp
        tst_blackboxbase.h
        tst_blackboxapple.cpp
        tst_blackboxapple.h
    )

add_qbs_test(blackbox-baremetal
    SOURCES
        ../shared.h
        tst_blackboxbase.cpp
        tst_blackboxbase.h
        tst_blackboxbaremetal.cpp
        tst_blackboxbaremetal.h
    )

add_qbs_test(blackbox-clangdb
    SOURCES
        ../shared.h
        tst_blackboxbase.cpp
        tst_blackboxbase.h
        tst_clangdb.cpp
        tst_clangdb.h
    )

add_qbs_test(blackbox-java
    SOURCES
        ../shared.h
        tst_blackboxbase.cpp
        tst_blackboxbase.h
        tst_blackboxjava.cpp
        tst_blackboxjava.h
    )

add_qbs_test(blackbox-joblimits
    SOURCES
        ../shared.h
        tst_blackboxbase.cpp
        tst_blackboxbase.h
        tst_blackboxjoblimits.cpp
    )

add_qbs_test(blackbox-qt
    SOURCES
        ../shared.h
        tst_blackboxbase.cpp
        tst_blackboxbase.h
        tst_blackboxqt.cpp
        tst_blackboxqt.h
    )

add_qbs_test(blackbox-windows
    SOURCES
        ../shared.h
        tst_blackboxbase.cpp
        tst_blackboxbase.h
        tst_blackboxwindows.cpp
        tst_blackboxwindows.h
    )
