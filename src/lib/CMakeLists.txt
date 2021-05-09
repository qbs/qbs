if (QBS_USE_BUNDLED_QT_SCRIPT OR NOT Qt5Script_FOUND)
    add_subdirectory(scriptengine)
else()
    add_library(qbsscriptengine ALIAS Qt5::Script)
endif()

add_subdirectory(corelib)
add_subdirectory(msbuild)
