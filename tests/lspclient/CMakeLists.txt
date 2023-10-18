add_qbs_app(qbs_lspclient
    DEPENDS
      Qt${QT_VERSION_MAJOR}::Core
      Qt${QT_VERSION_MAJOR}::Network
      qbscore
      qtclsp
    SOURCES lspclient.cpp
    )
