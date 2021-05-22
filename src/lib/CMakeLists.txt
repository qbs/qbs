if (QBS_USE_BUNDLED_QT_SCRIPT OR NOT Qt5Script_FOUND)
    add_subdirectory(scriptengine)
endif()

add_subdirectory(pkgconfig)
add_subdirectory(corelib)
add_subdirectory(msbuild)
