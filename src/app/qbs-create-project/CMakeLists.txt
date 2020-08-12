set(SOURCES
    createproject.cpp
    createproject.h
    create-project-main.cpp
    )

add_qbs_app(qbs-create-project
    DEPENDS corelib logginglib
    SOURCES ${SOURCES}
    )
