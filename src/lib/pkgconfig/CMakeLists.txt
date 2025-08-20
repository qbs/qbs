set(SOURCES
    pcpackage.cpp
    pcpackage.h
    pcparser.cpp
    pcparser.h
    pkgconfig.cpp
    pkgconfig.h
)
list_transform_prepend(SOLUTION_SOURCES solution/)

set(QBS_PKGCONFIG_PUBLIC_DEPENDS "")
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    set(QBS_PKGCONFIG_PUBLIC_DEPENDS "stdc++fs")
endif()

set(PKG_CONFIG_PC_PATH "${CMAKE_INSTALL_PREFIX}/${QBS_LIBDIR_NAME}/pkgconfig:${CMAKE_INSTALL_PREFIX}/share/pkgconfig")

if (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(PKG_CONFIG_PC_PATH "${PKG_CONFIG_PC_PATH}:/usr/local/libdata/pkgconfig/:/usr/libdata/pkgconfig/:/usr/local/share/pkgconfig/")
elseif (UNIX)
    set(PKG_CONFIG_PC_PATH "${PKG_CONFIG_PC_PATH}:/usr/lib/pkgconfig/:/usr/share/pkgconfig/")
endif()

message(${PKG_CONFIG_PC_PATH})

add_qbs_library(qbspkgconfig
    STATIC
    DEFINES
        "PKG_CONFIG_PC_PATH=\"${PKG_CONFIG_PC_PATH}\""
        "PKG_CONFIG_SYSTEM_LIBRARY_PATH=\"/usr/${QBS_LIBDIR_NAME}\""
    PUBLIC_DEFINES
        "QBS_PC_WITH_QT_SUPPORT=1"
    PUBLIC_DEPENDS ${QBS_PKGCONFIG_PUBLIC_DEPENDS}
    SOURCES ${SOURCES}
)
