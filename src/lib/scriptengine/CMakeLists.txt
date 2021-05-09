set(QT_SCRIPT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../shared/qtscript/src/")

if (UNIX)
    set(_USE_SYSTEM_MALLOC NO)
else()
    set(_USE_SYSTEM_MALLOC YES)
endif (UNIX)

set(JAVASCRIPT_CORE_INCLUDES
    assembler
    bytecode
    bytecompiler
    debugger
    interpreter
    jit
    parser
    pcre
    profiler
    runtime
    wrec
    wtf
    wtf/unicode
    yarr
    API
    ForwardingHeaders
    generated
)
list_transform_prepend(JAVASCRIPT_CORE_INCLUDES "${QT_SCRIPT_PATH}/3rdparty/javascriptcore/JavaScriptCore/")

set(QT_SCRIPT_PUBLIC_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/include"
)

set(QT_SCRIPT_INCLUDES
    "${QT_SCRIPT_PATH}/3rdparty/javascriptcore"
    "${QT_SCRIPT_PATH}/3rdparty/javascriptcore/JavaScriptCore"
    "${QT_SCRIPT_PATH}/script"
    "${QT_SCRIPT_PATH}/script/api"
    "${QT_SCRIPT_PATH}/script/parser"
    "${CMAKE_CURRENT_BINARY_DIR}/include/QtScript/${Qt5_VERSION}/QtScript"
)

set(QT_SCRIPT_DEFINES
    "QT_BUILD_SCRIPT_LIB"
    "QT_NO_USING_NAMESPACE"
    "JSC=QTJSC"
    "jscyyparse=qtjscyyparse"
    "jscyylex=qtjscyylex"
    "jscyyerror=qtjscyyerror"
    "WTF=QTWTF"
    "LOG_DISABLED=1"
    "WTF_USE_JAVASCRIPTCORE_BINDINGS=1"
    "WTF_CHANGES=1"
    "JS_NO_EXPORT"

    # JavaScriptCore
    "BUILDING_QT__"
    "BUILDING_JavaScriptCore"
    "BUILDING_WTF"
    "ENABLE_JIT=0"
    "ENABLE_YARR_JIT=0"
    "ENABLE_YARR=0"

    # WebKit
    "WTF_USE_ACCELERATED_COMPOSITING"
)

if (APPLE)
    list(APPEND QT_SCRIPT_DEFINES "ENABLE_JSC_MULTIPLE_THREADS=0")
elseif(WIN32)
    list(APPEND QT_SCRIPT_DEFINES "NOMINMAX" "_CRT_RAND_S")
endif()

if (MSVC)
    set(QT_SCRIPT_CXX_FLAGS "/wd4291" "/wd4344" "/wd4396" "/wd4503" "/wd4800" "/wd4819" "/wd4996")
else()
    set(QT_SCRIPT_CXX_FLAGS "-fno-strict-aliasing" "-w"
        "-Wall" "-Wreturn-type" "-Wcast-align" "-Wchar-subscripts"
        "-Wformat-security" "-Wreturn-type" "-Wno-unused-parameter"
        "-Wno-sign-compare" "-Wno-switch" "-Wno-switch-enum" "-Wundef"
        "-Wmissing-noreturn" "-Winit-self" "-Wno-deprecated" "-Wno-suggest-attribute=noreturn"
        "-Wno-nonnull-compare" "-pthread")
endif()

set(QT_SCRIPT_PUBLIC_DEPENDS Qt5::Core)
if (UNIX)
    list(APPEND QT_SCRIPT_PUBLIC_DEPENDS "pthread")
elseif(WIN32)
    list(APPEND QT_SCRIPT_PUBLIC_DEPENDS "winmm")
endif()

set(PCRE_SOURCES
    pcre_compile.cpp
    pcre_exec.cpp
    pcre_tables.cpp
    pcre_ucp_searchfuncs.cpp
    pcre_xclass.cpp
)
list_transform_prepend(PCRE_SOURCES "${QT_SCRIPT_PATH}/3rdparty/javascriptcore/JavaScriptCore/pcre/")

if (NOT _USE_SYSTEM_MALLOC)
    set(SYSTEM_MALLOC_SOURCES
        wtf/TCSystemAlloc.cpp
    )
    list_transform_prepend(SYSTEM_MALLOC_SOURCES "${QT_SCRIPT_PATH}/3rdparty/javascriptcore/JavaScriptCore/")

    list(APPEND QT_SCRIPT_DEFINES "USE_SYSTEM_MALLOC")
else()
    set(SYSTEM_MALLOC_SOURCES "")
endif()

set(JAVASCRIPT_CORE_SOURCES
    API/JSBase.cpp
    API/JSCallbackConstructor.cpp
    API/JSCallbackFunction.cpp
    API/JSCallbackObject.cpp
    API/JSClassRef.cpp
    API/JSContextRef.cpp
    API/JSObjectRef.cpp
    API/JSStringRef.cpp
    API/JSValueRef.cpp
    API/OpaqueJSString.cpp
    assembler/ARMAssembler.cpp
    assembler/MacroAssemblerARM.cpp
    bytecode/CodeBlock.cpp
    bytecode/JumpTable.cpp
    bytecode/Opcode.cpp
    bytecode/SamplingTool.cpp
    bytecode/StructureStubInfo.cpp
    bytecompiler/BytecodeGenerator.cpp
    bytecompiler/NodesCodegen.cpp
    debugger/DebuggerActivation.cpp
    debugger/DebuggerCallFrame.cpp
    debugger/Debugger.cpp
    generated/Grammar.cpp
    interpreter/CallFrame.cpp
    interpreter/Interpreter.cpp
    interpreter/RegisterFile.cpp
    parser/Lexer.cpp
    parser/Nodes.cpp
    parser/ParserArena.cpp
    parser/Parser.cpp
    profiler/Profile.cpp
    profiler/ProfileGenerator.cpp
    profiler/ProfileNode.cpp
    profiler/Profiler.cpp
    runtime/ArgList.cpp
    runtime/Arguments.cpp
    runtime/ArrayConstructor.cpp
    runtime/ArrayPrototype.cpp
    runtime/BooleanConstructor.cpp
    runtime/BooleanObject.cpp
    runtime/BooleanPrototype.cpp
    runtime/CallData.cpp
    runtime/Collector.cpp
    runtime/CommonIdentifiers.cpp
    runtime/Completion.cpp
    runtime/ConstructData.cpp
    runtime/DateConstructor.cpp
    runtime/DateConversion.cpp
    runtime/DateInstance.cpp
    runtime/DatePrototype.cpp
    runtime/ErrorConstructor.cpp
    runtime/Error.cpp
    runtime/ErrorInstance.cpp
    runtime/ErrorPrototype.cpp
    runtime/ExceptionHelpers.cpp
    runtime/Executable.cpp
    runtime/FunctionConstructor.cpp
    runtime/FunctionPrototype.cpp
    runtime/GetterSetter.cpp
    runtime/GlobalEvalFunction.cpp
    runtime/Identifier.cpp
    runtime/InitializeThreading.cpp
    runtime/InternalFunction.cpp
    runtime/JSActivation.cpp
    runtime/JSAPIValueWrapper.cpp
    runtime/JSArray.cpp
    runtime/JSByteArray.cpp
    runtime/JSCell.cpp
    runtime/JSFunction.cpp
    runtime/JSGlobalData.cpp
    runtime/JSGlobalObject.cpp
    runtime/JSGlobalObjectFunctions.cpp
    runtime/JSImmediate.cpp
    runtime/JSLock.cpp
    runtime/JSNotAnObject.cpp
    runtime/JSNumberCell.cpp
    runtime/JSObject.cpp
    runtime/JSONObject.cpp
    runtime/JSPropertyNameIterator.cpp
    runtime/JSStaticScopeObject.cpp
    runtime/JSString.cpp
    runtime/JSValue.cpp
    runtime/JSVariableObject.cpp
    runtime/JSWrapperObject.cpp
    runtime/LiteralParser.cpp
    runtime/Lookup.cpp
    runtime/MarkStackPosix.cpp
    runtime/MarkStackSymbian.cpp
    runtime/MarkStackWin.cpp
    runtime/MarkStack.cpp
    runtime/MathObject.cpp
    runtime/NativeErrorConstructor.cpp
    runtime/NativeErrorPrototype.cpp
    runtime/NumberConstructor.cpp
    runtime/NumberObject.cpp
    runtime/NumberPrototype.cpp
    runtime/ObjectConstructor.cpp
    runtime/ObjectPrototype.cpp
    runtime/Operations.cpp
    runtime/PropertyDescriptor.cpp
    runtime/PropertyNameArray.cpp
    runtime/PropertySlot.cpp
    runtime/PrototypeFunction.cpp
    runtime/RegExpConstructor.cpp
    runtime/RegExp.cpp
    runtime/RegExpObject.cpp
    runtime/RegExpPrototype.cpp
    runtime/ScopeChain.cpp
    runtime/SmallStrings.cpp
    runtime/StringConstructor.cpp
    runtime/StringObject.cpp
    runtime/StringPrototype.cpp
    runtime/StructureChain.cpp
    runtime/Structure.cpp
    runtime/TimeoutChecker.cpp
    runtime/UString.cpp
    runtime/UStringImpl.cpp
    wtf/Assertions.cpp
    wtf/ByteArray.cpp
    wtf/CurrentTime.cpp
    wtf/DateMath.cpp
    wtf/dtoa.cpp
    wtf/FastMalloc.cpp
    wtf/HashTable.cpp
    wtf/MainThread.cpp
    wtf/qt/MainThreadQt.cpp
    wtf/qt/ThreadingQt.cpp
    wtf/RandomNumber.cpp
    wtf/RefCountedLeakCounter.cpp
    wtf/ThreadingNone.cpp
    wtf/Threading.cpp
    wtf/TypeTraits.cpp
    wtf/unicode/CollatorDefault.cpp
    wtf/unicode/icu/CollatorICU.cpp
    wtf/unicode/UTF8.cpp
)
list_transform_prepend(JAVASCRIPT_CORE_SOURCES "${QT_SCRIPT_PATH}/3rdparty/javascriptcore/JavaScriptCore/")

set(API_SOURCES
    qscriptable.cpp
    qscriptable.h
    qscriptable_p.h
    qscriptclass.cpp
    qscriptclass.h
    qscriptclasspropertyiterator.cpp
    qscriptclasspropertyiterator.h
    qscriptcontext.cpp
    qscriptcontext.h
    qscriptcontextinfo.cpp
    qscriptcontextinfo.h
    qscriptcontext_p.h
    qscriptengineagent.cpp
    qscriptengineagent.h
    qscriptengineagent_p.h
    qscriptengine.cpp
    qscriptengine.h
    qscriptengine_p.h
    qscriptextensioninterface.h
    qscriptextensionplugin.cpp
    qscriptextensionplugin.h
    qscriptprogram.cpp
    qscriptprogram.h
    qscriptprogram_p.h
    qscriptstring.cpp
    qscriptstring.h
    qscriptstring_p.h
    qscriptvalue.cpp
    qscriptvalue.h
    qscriptvalueiterator.cpp
    qscriptvalueiterator.h
    qscriptvalue_p.h
    qtscriptglobal.h
)
list_transform_prepend(API_SOURCES "${QT_SCRIPT_PATH}/script/api/")

set(BRIDGE_SOURCES
    qscriptactivationobject.cpp
    qscriptactivationobject_p.h
    qscriptclassobject.cpp
    qscriptclassobject_p.h
    qscriptfunction.cpp
    qscriptfunction_p.h
    qscriptglobalobject.cpp
    qscriptglobalobject_p.h
    qscriptobject.cpp
    qscriptobject_p.h
    qscriptqobject.cpp
    qscriptqobject_p.h
    qscriptstaticscopeobject.cpp
    qscriptstaticscopeobject_p.h
    qscriptvariant.cpp
    qscriptvariant_p.h
)
list_transform_prepend(BRIDGE_SOURCES "${QT_SCRIPT_PATH}/script/bridge/")

set(PARSER_SOURCES
    qscriptast.cpp
    qscriptastfwd_p.h
    qscriptast_p.h
    qscriptastvisitor.cpp
    qscriptastvisitor_p.h
    qscriptgrammar.cpp
    qscriptgrammar_p.h
    qscriptlexer.cpp
    qscriptlexer_p.h
    qscriptsyntaxchecker.cpp
    qscriptsyntaxchecker_p.h
)
list_transform_prepend(PARSER_SOURCES "${QT_SCRIPT_PATH}/script/parser/")

find_package(Perl)

get_target_property(_QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
get_filename_component(_QT_BIN_DIRECTORY ${_QT_QMAKE_EXECUTABLE} DIRECTORY)

file(GLOB API_HEADERS "${QT_SCRIPT_PATH}/script/api/*.h")

message("${QT_SCRIPT_PATH}")

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/include/QtScript/qscriptengine.h"
    COMMAND ${PERL_EXECUTABLE} "${_QT_BIN_DIRECTORY}/syncqt.pl"
        -minimal
        -version "${Qt5_VERSION}"
        -outdir ${CMAKE_CURRENT_BINARY_DIR}
        "${QT_SCRIPT_PATH}/.."
    BYPRODUCTS
    DEPENDS ${API_HEADERS}
    )

add_qbs_library(qbsscriptengine
    STATIC
    DEFINES ${QT_SCRIPT_DEFINES}
    DEPENDS Qt5::CorePrivate
    PUBLIC_DEPENDS ${QT_SCRIPT_PUBLIC_DEPENDS}
    INCLUDES ${QT_SCRIPT_PUBLIC_INCLUDES} ${QT_SCRIPT_INCLUDES} ${JAVASCRIPT_CORE_INCLUDES}
    PUBLIC_INCLUDES ${QT_SCRIPT_PUBLIC_INCLUDES}
    SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/include/QtScript/qscriptengine.h"
        ${PCRE_SOURCES}
        ${SYSTEM_MALLOC_SOURCES}
        ${JAVASCRIPT_CORE_SOURCES}
        ${API_SOURCES}
        ${BRIDGE_SOURCES}
        ${PARSER_SOURCES}
    )
set_property(TARGET qbsscriptengine PROPERTY CXX_STANDARD 14)
target_compile_options(qbsscriptengine PRIVATE ${QT_SCRIPT_CXX_FLAGS})
