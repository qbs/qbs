add_qbs_library(qbsquickjs
    STATIC
    SOURCES
        builtin-array-fromasync.h
        cutils.c cutils.h
        libbf.c libbf.h
        libregexp-opcode.h
        libregexp.c libregexp.h
        libunicode-table.h
        libunicode.c libunicode.h
        list.h
        quickjs-atom.h
        quickjs-c-atomics.h
        quickjs-opcode.h
        quickjs.c quickjs.h
        xsum.c xsum.h
    DEFINES
        "CONFIG_VERSION=\"2021-03-27\""
)
set_property(TARGET qbsquickjs PROPERTY C_STANDARD 11)
if(QBS_QUICKJS_LEAK_CHECK)
  add_definitions(-DDUMP_LEAKS)
endif()
target_include_directories(
    qbsquickjs
    SYSTEM
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(qbsquickjsheaders INTERFACE)
target_include_directories(
    qbsquickjsheaders
    SYSTEM
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
)
if(MSVC)
  target_compile_options(qbsquickjs PUBLIC /wd4018 /wd4334 /wd4101 /wd4146 /wd4244 /wd4267 /wd4996)
endif()
