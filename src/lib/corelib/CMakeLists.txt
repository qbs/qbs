set(QBS_HEADERS qbs.h)

set(FILE_UPDATE_SOURCES
    changeset.cpp
    changeset.h
    projectfileupdater.cpp
    projectfileupdater.h
    qmljsrewriter.cpp
    qmljsrewriter.h
    )
list_transform_prepend(FILE_UPDATE_SOURCES api/)

set(API_SOURCES
    internaljobs.cpp
    internaljobs.h
    jobs.cpp
    languageinfo.cpp
    project.cpp
    project_p.h
    projectdata.cpp
    projectdata_p.h
    propertymap_p.h
    rulecommand.cpp
    rulecommand_p.h
    runenvironment.cpp
    transformerdata.cpp
    transformerdata_p.h
    )
list_transform_prepend(API_SOURCES api/)

set(API_HEADERS
    jobs.h
    languageinfo.h
    project.h
    projectdata.h
    rulecommand.h
    runenvironment.h
    transformerdata.h
    )
list_transform_prepend(API_HEADERS api/)

set(BUILD_GRAPH_SOURCES
    abstractcommandexecutor.cpp
    abstractcommandexecutor.h
    artifact.cpp
    artifact.h
    artifactcleaner.cpp
    artifactcleaner.h
    artifactsscriptvalue.cpp
    artifactsscriptvalue.h
    artifactvisitor.cpp
    artifactvisitor.h
    buildgraph.cpp
    buildgraph.h
    buildgraphnode.cpp
    buildgraphnode.h
    buildgraphloader.cpp
    buildgraphloader.h
    buildgraphvisitor.h
    cycledetector.cpp
    cycledetector.h
    dependencyparametersscriptvalue.cpp
    dependencyparametersscriptvalue.h
    depscanner.cpp
    depscanner.h
    emptydirectoriesremover.cpp
    emptydirectoriesremover.h
    environmentscriptrunner.cpp
    environmentscriptrunner.h
    executor.cpp
    executor.h
    executorjob.cpp
    executorjob.h
    filedependency.cpp
    filedependency.h
    inputartifactscanner.cpp
    inputartifactscanner.h
    jscommandexecutor.cpp
    jscommandexecutor.h
    nodeset.cpp
    nodeset.h
    nodetreedumper.cpp
    nodetreedumper.h
    processcommandexecutor.cpp
    processcommandexecutor.h
    productbuilddata.cpp
    productbuilddata.h
    productinstaller.cpp
    productinstaller.h
    projectbuilddata.cpp
    projectbuilddata.h
    qtmocscanner.cpp
    qtmocscanner.h
    rawscanneddependency.cpp
    rawscanneddependency.h
    rawscanresults.cpp
    rawscanresults.h
    requestedartifacts.cpp
    requestedartifacts.h
    requesteddependencies.cpp
    requesteddependencies.h
    rescuableartifactdata.h
    rulecommands.cpp
    rulecommands.h
    rulegraph.cpp
    rulegraph.h
    rulenode.cpp
    rulenode.h
    rulesapplicator.cpp
    rulesapplicator.h
    rulesevaluationcontext.cpp
    rulesevaluationcontext.h
    timestampsupdater.cpp
    timestampsupdater.h
    transformer.cpp
    transformer.h
    transformerchangetracking.cpp
    transformerchangetracking.h
    )
list_transform_prepend(BUILD_GRAPH_SOURCES buildgraph/)

set(BUILD_GRAPH_HEADERS buildgraph/forward_decls.h)

set(GENERATORS_SOURCES
    generatableprojectiterator.cpp
    generatableprojectiterator.h
    generator.cpp
    generatordata.cpp
    generatorutils.cpp
    generatorutils.h
    generatorversioninfo.cpp
    generatorversioninfo.h
    igeneratableprojectvisitor.h
    ixmlnodevisitor.h
    xmlproject.cpp
    xmlproject.h
    xmlprojectwriter.cpp
    xmlprojectwriter.h
    xmlproperty.cpp
    xmlproperty.h
    xmlpropertygroup.cpp
    xmlpropertygroup.h
    xmlworkspace.cpp
    xmlworkspace.h
    xmlworkspacewriter.cpp
    xmlworkspacewriter.h
    )
list_transform_prepend(GENERATORS_SOURCES generators/)

set(GENERATORS_HEADERS generators/generator.h generators/generatordata.h)

set(JS_EXTENSIONS_SOURCES
    environmentextension.cpp
    file.cpp
    fileinfoextension.cpp
    host.cpp
    jsextensions.cpp
    jsextensions.h
    moduleproperties.cpp
    moduleproperties.h
    pkgconfigjs.cpp
    pkgconfigjs.h
    process.cpp
    temporarydir.cpp
    textfile.cpp
    binaryfile.cpp
    utilitiesextension.cpp
    domxml.cpp
    )
list_transform_prepend(JS_EXTENSIONS_SOURCES jsextensions/)

if(APPLE)
    set(JS_EXTENSIONS_MACOS_SOURCES
        propertylist_darwin.mm
        propertylistutils.h
        propertylistutils.mm
        )
else()
    set(JS_EXTENSIONS_MACOS_SOURCES propertylist.cpp)
endif()
list_transform_prepend(JS_EXTENSIONS_MACOS_SOURCES jsextensions/)

set(LANGUAGE_SOURCES
    artifactproperties.cpp
    artifactproperties.h
    asttools.cpp
    asttools.h
    builtindeclarations.cpp
    builtindeclarations.h
    deprecationinfo.h
    evaluator.cpp
    evaluator.h
    filecontext.cpp
    filecontext.h
    filecontextbase.cpp
    filecontextbase.h
    filetags.cpp
    filetags.h
    identifiersearch.cpp
    identifiersearch.h
    item.cpp
    item.h
    itemdeclaration.cpp
    itemdeclaration.h
    itemobserver.h
    itempool.cpp
    itempool.h
    itemtype.h
    jsimports.h
    language.cpp
    language.h
    moduleproviderinfo.h
    preparescriptobserver.cpp
    preparescriptobserver.h
    property.cpp
    property.h
    propertydeclaration.cpp
    propertydeclaration.h
    propertymapinternal.cpp
    propertymapinternal.h
    qualifiedid.cpp
    qualifiedid.h
    resolvedfilecontext.cpp
    resolvedfilecontext.h
    scriptengine.cpp
    scriptengine.h
    scriptimporter.cpp
    scriptimporter.h
    scriptpropertyobserver.cpp
    scriptpropertyobserver.h
    value.cpp
    value.h
    )
list_transform_prepend(LANGUAGE_SOURCES language/)

set(LANGUAGE_HEADERS language/forward_decls.h)

set(LOADER_SOURCES
    astimportshandler.cpp
    astimportshandler.h
    astpropertiesitemhandler.cpp
    astpropertiesitemhandler.h
    dependenciesresolver.cpp
    dependenciesresolver.h
    groupshandler.cpp
    groupshandler.h
    itemreader.cpp
    itemreader.h
    itemreaderastvisitor.cpp
    itemreaderastvisitor.h
    itemreadervisitorstate.cpp
    itemreadervisitorstate.h
    loaderutils.cpp
    loaderutils.h
    localprofiles.cpp
    localprofiles.h
    moduleinstantiator.cpp
    moduleinstantiator.h
    moduleloader.cpp
    moduleloader.h
    modulepropertymerger.cpp
    modulepropertymerger.h
    moduleproviderloader.cpp
    moduleproviderloader.h
    probesresolver.cpp
    probesresolver.h
    productitemmultiplexer.cpp
    productitemmultiplexer.h
    productresolver.cpp
    productresolver.h
    productscollector.cpp
    productscollector.h
    productsresolver.cpp
    productsresolver.h
    projectresolver.cpp
    projectresolver.h
    )
list_transform_prepend(LOADER_SOURCES loader/)

set(LOGGING_SOURCES
    categories.cpp
    categories.h
    ilogsink.cpp
    logger.cpp
    logger.h
    translator.h
    )
list_transform_prepend(LOGGING_SOURCES logging/)

set(LOGGING_HEADERS logging/ilogsink.h)

set(PARSER_SOURCES
    qmlerror.cpp
    qmlerror.h
    qmljsast.cpp
    qmljsast_p.h
    qmljsastfwd_p.h
    qmljsastvisitor.cpp
    qmljsastvisitor_p.h
    qmljsengine_p.cpp
    qmljsengine_p.h
    qmljsglobal_p.h
    qmljsgrammar.cpp
    qmljsgrammar_p.h
    qmljskeywords_p.h
    qmljslexer.cpp
    qmljslexer_p.h
    qmljsmemorypool_p.h
    qmljsparser.cpp
    qmljsparser_p.h
    )
list_transform_prepend(PARSER_SOURCES parser/)

set(TOOLS_SOURCES
    architectures.cpp
    buildgraphlocker.cpp
    buildgraphlocker.h
    buildoptions.cpp
    clangclinfo.cpp
    clangclinfo.h
    cleanoptions.cpp
    codelocation.cpp
    commandechomode.cpp
    deprecationwarningmode.cpp
    dynamictypecheck.h
    error.cpp
    executablefinder.cpp
    executablefinder.h
    fileinfo.cpp
    fileinfo.h
    filesaver.cpp
    filesaver.h
    filetime.cpp
    filetime.h
    generateoptions.cpp
    hostosinfo.h
    id.cpp
    id.h
    iosutils.h
    joblimits.cpp
    jsliterals.cpp
    jsliterals.h
    jsonhelper.h
    installoptions.cpp
    launcherinterface.cpp
    launcherinterface.h
    launcherpackets.cpp
    launcherpackets.h
    launchersocket.cpp
    launchersocket.h
    msvcinfo.cpp
    msvcinfo.h
    pathutils.h
    pimpl.h
    persistence.cpp
    persistence.h
    porting.h
    preferences.cpp
    processresult.cpp
    processresult_p.h
    processutils.cpp
    processutils.h
    profile.cpp
    profiling.cpp
    profiling.h
    progressobserver.cpp
    progressobserver.h
    projectgeneratormanager.cpp
    propagate_const.h
    qbsassert.cpp
    qbsassert.h
    qbspluginmanager.cpp
    qbspluginmanager.h
    qbsprocess.cpp
    qbsprocess.h
    qttools.cpp
    qttools.h
    scannerpluginmanager.cpp
    scannerpluginmanager.h
    scripttools.cpp
    scripttools.h
    set.h
    settings.cpp
    settingscreator.cpp
    settingscreator.h
    settingsmodel.cpp
    settingsrepresentation.cpp
    setupprojectparameters.cpp
    shellutils.cpp
    shellutils.h
    stlutils.h
    stringconstants.h
    stringutils.h
    toolchains.cpp
    version.cpp
    visualstudioversioninfo.cpp
    visualstudioversioninfo.h
    vsenvironmentdetector.cpp
    vsenvironmentdetector.h
    weakpointer.h
    )
list_transform_prepend(TOOLS_SOURCES tools/)

set(TOOLS_HEADERS
    architectures.h
    buildoptions.h
    cleanoptions.h
    codelocation.h
    commandechomode.h
    deprecationwarningmode.h
    error.h
    generateoptions.h
    installoptions.h
    joblimits.h
    preferences.h
    processresult.h
    profile.h
    projectgeneratormanager.h
    qbs_export.h
    settings.h
    settingsmodel.h
    settingsrepresentation.h
    setupprojectparameters.h
    toolchains.h
    version.h
    )
list_transform_prepend(TOOLS_HEADERS tools/)

set(EXTERNAL_DEPENDS "")
if(APPLE)
    set(TOOLS_MACOS_SOURCES
        applecodesignutils.cpp
        applecodesignutils.h
        )
    list_transform_prepend(TOOLS_MACOS_SOURCES tools/)
    set(EXTERNAL_DEPENDS "-framework Foundation" "-framework Security")
endif()

if(WIN32)
    set(EXTERNAL_DEPENDS "psapi" "shell32")
endif()

add_qbs_library(qbscore
    DEFINES
        "QBS_VERSION=\"${QBS_VERSION}\""
        "QBS_RELATIVE_LIBEXEC_PATH=\"${QBS_RELATIVE_LIBEXEC_PATH}\""
        "QBS_LIBRARY"
        ${QBS_UNIT_TESTS_DEFINES}
    DEPENDS
        Qt${QT_VERSION_MAJOR}::CorePrivate
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Xml
        Qt6Core5Compat
        qbspkgconfig
        qbsquickjs
    PUBLIC_DEPENDS
        Qt${QT_VERSION_MAJOR}::Core
        ${EXTERNAL_DEPENDS}
    INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    SOURCES
        ${QBS_HEADERS}
        ${FILE_UPDATE_SOURCES}
        ${API_SOURCES}
        ${API_HEADERS}
        ${BUILD_GRAPH_SOURCES}
        ${BUILD_GRAPH_HEADERS}
        ${GENERATORS_SOURCES}
        ${GENERATORS_HEADERS}
        ${JS_EXTENSIONS_SOURCES}
        ${JS_EXTENSIONS_MACOS_SOURCES}
        ${LANGUAGE_SOURCES}
        ${LANGUAGE_HEADERS}
        ${LOADER_SOURCES}
        ${LOGGING_SOURCES}
        ${LOGGING_HEADERS}
        ${PARSER_SOURCES}
        ${TOOLS_SOURCES}
        ${TOOLS_HEADERS}
        ${TOOLS_MACOS_SOURCES}
    )

# not sure if there's a better way to do this
if(INSTALL_PUBLIC_HEADERS)
    install(FILES ${QBS_HEADERS} DESTINATION ${QBS_HEADERS_INSTALL_DIR})
    install(FILES ${API_HEADERS} DESTINATION ${QBS_HEADERS_INSTALL_DIR}/api)
    install(FILES ${BUILD_GRAPH_HEADERS} DESTINATION ${QBS_HEADERS_INSTALL_DIR}/buildgraph)
    install(FILES ${GENERATORS_HEADERS} DESTINATION ${QBS_HEADERS_INSTALL_DIR}/generators)
    install(FILES ${LOGGING_HEADERS} DESTINATION ${QBS_HEADERS_INSTALL_DIR}/logging)
    install(FILES ${LANGUAGE_HEADERS} DESTINATION ${QBS_HEADERS_INSTALL_DIR}/language)
    install(FILES ${TOOLS_HEADERS} DESTINATION ${QBS_HEADERS_INSTALL_DIR}/tools)
endif()
