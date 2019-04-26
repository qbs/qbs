import qbs
import qbs.File
import qbs.FileInfo
import qbs.Process

Project {
    QbsLibrary {
        condition: qbsbuildconfig.useBundledQtScript || !Qt.script.present
        Depends {
            name: "Qt.script"
            condition: !qbsbuildconfig.useBundledQtScript
            required: false
        }
        Depends { name: "QtScriptFwdHeaders" }
        Depends { name: "cpp" }
        Depends { name: "Qt"; submodules: ["core-private"] }
        type: ["staticlibrary"]
        name: "qbsscriptengine"

        generatePkgConfigFile: false
        generateQbsModule: false

        property bool useSystemMalloc: !qbs.targetOS.contains("macos")
                                       && !qbs.targetOS.contains("unix")
        property string qtscriptPath: "../../shared/qtscript/src/"

        cpp.includePaths: {
            var result = base.concat(
                ".",
                "include"
            );

            var jscBaseDir = qtscriptPath + "3rdparty/javascriptcore";
            result.push(jscBaseDir);
            jscBaseDir += "/JavaScriptCore";
            result.push(jscBaseDir);

            var jscSubDirs = [
                "assembler",
                "bytecode",
                "bytecompiler",
                "debugger",
                "interpreter",
                "jit",
                "parser",
                "pcre",
                "profiler",
                "runtime",
                "wrec",
                "wtf",
                "wtf/unicode",
                "yarr",
                "API",
                "ForwardingHeaders",
                "generated"];
            result = result.concat(jscSubDirs.map(function(s) { return jscBaseDir + '/' + s; }));

            result.push(qtscriptPath + "script");
            result.push(qtscriptPath + "script/api");
            result.push(qtscriptPath + "script/parser");
            result = result.concat(QtScriptFwdHeaders.publicIncludePaths,
                                   QtScriptFwdHeaders.privateIncludePaths);
            return result;
        }
        cpp.defines: {
            var result = base.concat([
                "QT_BUILD_SCRIPT_LIB", "QT_NO_USING_NAMESPACE",
                "JSC=QTJSC", "jscyyparse=qtjscyyparse", "jscyylex=qtjscyylex",
                "jscyyerror=qtjscyyerror",
                "WTF=QTWTF",
                "LOG_DISABLED=1",
                "WTF_USE_JAVASCRIPTCORE_BINDINGS=1", "WTF_CHANGES=1",
                "JS_NO_EXPORT"]);
            if (qbs.buildVariant != "debug")
                result.push("NDEBUG");
            if (qbs.targetOS.contains("macos"))
                result.push("ENABLE_JSC_MULTIPLE_THREADS=0");

            // JavaScriptCore
            result.push("BUILDING_QT__", "BUILDING_JavaScriptCore", "BUILDING_WTF",
                        "ENABLE_JIT=0", "ENABLE_YARR_JIT=0", "ENABLE_YARR=0");
            if (qbs.targetOS.contains("windows")) {
                // Prevent definition of min, max macros in windows.h
                result.push("NOMINMAX");
                // Enables rand_s
                result.push("_CRT_RAND_S");
            }

            // WebKit
            result.push("WTF_USE_ACCELERATED_COMPOSITING");
            if (useSystemMalloc)
                result.push("USE_SYSTEM_MALLOC");
            return result;
        }
        cpp.cxxFlags: {
            var result = base;
            if (qbs.toolchain.contains("gcc")) {
                result.push("-fno-strict-aliasing",
                            "-Wall", "-Wreturn-type", "-Wcast-align", "-Wchar-subscripts",
                            "-Wformat-security", "-Wreturn-type", "-Wno-unused-parameter",
                            "-Wno-sign-compare", "-Wno-switch", "-Wno-switch-enum", "-Wundef",
                            "-Wmissing-noreturn", "-Winit-self", "-Wno-deprecated",
                            "-Wno-suggest-attribute=noreturn", "-Wno-nonnull-compare");
            } else if (qbs.toolchain.contains("msvc")) {
                result.push("-wd4291", "-wd4344", "-wd4396", "-wd4503", "-wd4800", "-wd4819",
                            "-wd4996");
            }
            if (qbs.targetOS.contains("unix"))
                result.push("-pthread");

            return result;
        }
        cpp.warningLevel: "none"
        cpp.optimization: "fast" // We cannot afford -O0 for QtScript even in debug builds.
        Properties {
            condition: qbs.targetOS.contains("unix")
            cpp.dynamicLibraries: base.concat(["pthread"])
        }

        Group {
            name: "pcre"
            prefix: qtscriptPath + "3rdparty/javascriptcore/JavaScriptCore/pcre/"
            files: [
                "pcre_compile.cpp",
                "pcre_exec.cpp",
                "pcre_tables.cpp",
                "pcre_ucp_searchfuncs.cpp",
                "pcre_xclass.cpp",
            ]
        }

        Group {
            name: "system malloc replacement"
            prefix: qtscriptPath + "3rdparty/javascriptcore/JavaScriptCore/"
            condition: !useSystemMalloc
            files: [
                "wtf/TCSystemAlloc.cpp",
            ]
        }

        Group {
            name: "JavaScriptCore"
            prefix: qtscriptPath + "3rdparty/javascriptcore/JavaScriptCore/"
            files: [
                "API/JSBase.cpp",
                "API/JSCallbackConstructor.cpp",
                "API/JSCallbackFunction.cpp",
                "API/JSCallbackObject.cpp",
                "API/JSClassRef.cpp",
                "API/JSContextRef.cpp",
                "API/JSObjectRef.cpp",
                "API/JSStringRef.cpp",
                "API/JSValueRef.cpp",
                "API/OpaqueJSString.cpp",
                "assembler/ARMAssembler.cpp",
                "assembler/MacroAssemblerARM.cpp",
                "bytecode/CodeBlock.cpp",
                "bytecode/JumpTable.cpp",
                "bytecode/Opcode.cpp",
                "bytecode/SamplingTool.cpp",
                "bytecode/StructureStubInfo.cpp",
                "bytecompiler/BytecodeGenerator.cpp",
                "bytecompiler/NodesCodegen.cpp",
                "debugger/DebuggerActivation.cpp",
                "debugger/DebuggerCallFrame.cpp",
                "debugger/Debugger.cpp",
                "generated/Grammar.cpp",
                "interpreter/CallFrame.cpp",
                "interpreter/Interpreter.cpp",
                "interpreter/RegisterFile.cpp",
                "parser/Lexer.cpp",
                "parser/Nodes.cpp",
                "parser/ParserArena.cpp",
                "parser/Parser.cpp",
                "profiler/Profile.cpp",
                "profiler/ProfileGenerator.cpp",
                "profiler/ProfileNode.cpp",
                "profiler/Profiler.cpp",
                "runtime/ArgList.cpp",
                "runtime/Arguments.cpp",
                "runtime/ArrayConstructor.cpp",
                "runtime/ArrayPrototype.cpp",
                "runtime/BooleanConstructor.cpp",
                "runtime/BooleanObject.cpp",
                "runtime/BooleanPrototype.cpp",
                "runtime/CallData.cpp",
                "runtime/Collector.cpp",
                "runtime/CommonIdentifiers.cpp",
                "runtime/Completion.cpp",
                "runtime/ConstructData.cpp",
                "runtime/DateConstructor.cpp",
                "runtime/DateConversion.cpp",
                "runtime/DateInstance.cpp",
                "runtime/DatePrototype.cpp",
                "runtime/ErrorConstructor.cpp",
                "runtime/Error.cpp",
                "runtime/ErrorInstance.cpp",
                "runtime/ErrorPrototype.cpp",
                "runtime/ExceptionHelpers.cpp",
                "runtime/Executable.cpp",
                "runtime/FunctionConstructor.cpp",
                "runtime/FunctionPrototype.cpp",
                "runtime/GetterSetter.cpp",
                "runtime/GlobalEvalFunction.cpp",
                "runtime/Identifier.cpp",
                "runtime/InitializeThreading.cpp",
                "runtime/InternalFunction.cpp",
                "runtime/JSActivation.cpp",
                "runtime/JSAPIValueWrapper.cpp",
                "runtime/JSArray.cpp",
                "runtime/JSByteArray.cpp",
                "runtime/JSCell.cpp",
                "runtime/JSFunction.cpp",
                "runtime/JSGlobalData.cpp",
                "runtime/JSGlobalObject.cpp",
                "runtime/JSGlobalObjectFunctions.cpp",
                "runtime/JSImmediate.cpp",
                "runtime/JSLock.cpp",
                "runtime/JSNotAnObject.cpp",
                "runtime/JSNumberCell.cpp",
                "runtime/JSObject.cpp",
                "runtime/JSONObject.cpp",
                "runtime/JSPropertyNameIterator.cpp",
                "runtime/JSStaticScopeObject.cpp",
                "runtime/JSString.cpp",
                "runtime/JSValue.cpp",
                "runtime/JSVariableObject.cpp",
                "runtime/JSWrapperObject.cpp",
                "runtime/LiteralParser.cpp",
                "runtime/Lookup.cpp",
                "runtime/MarkStackPosix.cpp",
                "runtime/MarkStackSymbian.cpp",
                "runtime/MarkStackWin.cpp",
                "runtime/MarkStack.cpp",
                "runtime/MathObject.cpp",
                "runtime/NativeErrorConstructor.cpp",
                "runtime/NativeErrorPrototype.cpp",
                "runtime/NumberConstructor.cpp",
                "runtime/NumberObject.cpp",
                "runtime/NumberPrototype.cpp",
                "runtime/ObjectConstructor.cpp",
                "runtime/ObjectPrototype.cpp",
                "runtime/Operations.cpp",
                "runtime/PropertyDescriptor.cpp",
                "runtime/PropertyNameArray.cpp",
                "runtime/PropertySlot.cpp",
                "runtime/PrototypeFunction.cpp",
                "runtime/RegExpConstructor.cpp",
                "runtime/RegExp.cpp",
                "runtime/RegExpObject.cpp",
                "runtime/RegExpPrototype.cpp",
                "runtime/ScopeChain.cpp",
                "runtime/SmallStrings.cpp",
                "runtime/StringConstructor.cpp",
                "runtime/StringObject.cpp",
                "runtime/StringPrototype.cpp",
                "runtime/StructureChain.cpp",
                "runtime/Structure.cpp",
                "runtime/TimeoutChecker.cpp",
                "runtime/UString.cpp",
                "runtime/UStringImpl.cpp",
                "wtf/Assertions.cpp",
                "wtf/ByteArray.cpp",
                "wtf/CurrentTime.cpp",
                "wtf/DateMath.cpp",
                "wtf/dtoa.cpp",
                "wtf/FastMalloc.cpp",
                "wtf/HashTable.cpp",
                "wtf/MainThread.cpp",
                "wtf/qt/MainThreadQt.cpp",
                "wtf/qt/ThreadingQt.cpp",
                "wtf/RandomNumber.cpp",
                "wtf/RefCountedLeakCounter.cpp",
                "wtf/ThreadingNone.cpp",
                "wtf/Threading.cpp",
                "wtf/TypeTraits.cpp",
                "wtf/unicode/CollatorDefault.cpp",
                "wtf/unicode/icu/CollatorICU.cpp",
                "wtf/unicode/UTF8.cpp",
            ]
        }
        Group {
            name: "api"
            prefix: qtscriptPath + "script/api/"
            files: [
                "qscriptable.cpp",
                "qscriptable.h",
                "qscriptable_p.h",
                "qscriptclass.cpp",
                "qscriptclass.h",
                "qscriptclasspropertyiterator.cpp",
                "qscriptclasspropertyiterator.h",
                "qscriptcontext.cpp",
                "qscriptcontext.h",
                "qscriptcontextinfo.cpp",
                "qscriptcontextinfo.h",
                "qscriptcontext_p.h",
                "qscriptengineagent.cpp",
                "qscriptengineagent.h",
                "qscriptengineagent_p.h",
                "qscriptengine.cpp",
                "qscriptengine.h",
                "qscriptengine_p.h",
                "qscriptextensioninterface.h",
                "qscriptextensionplugin.cpp",
                "qscriptextensionplugin.h",
                "qscriptprogram.cpp",
                "qscriptprogram.h",
                "qscriptprogram_p.h",
                "qscriptstring.cpp",
                "qscriptstring.h",
                "qscriptstring_p.h",
                "qscriptvalue.cpp",
                "qscriptvalue.h",
                "qscriptvalueiterator.cpp",
                "qscriptvalueiterator.h",
                "qscriptvalue_p.h",
                "qtscriptglobal.h",
            ]
        }
        Group {
            name: "bridge"
            prefix: qtscriptPath + "script/bridge/"
            files: [
                "qscriptactivationobject.cpp",
                "qscriptactivationobject_p.h",
                "qscriptclassobject.cpp",
                "qscriptclassobject_p.h",
                "qscriptfunction.cpp",
                "qscriptfunction_p.h",
                "qscriptglobalobject.cpp",
                "qscriptglobalobject_p.h",
                "qscriptobject.cpp",
                "qscriptobject_p.h",
                "qscriptqobject.cpp",
                "qscriptqobject_p.h",
                "qscriptstaticscopeobject.cpp",
                "qscriptstaticscopeobject_p.h",
                "qscriptvariant.cpp",
                "qscriptvariant_p.h",
            ]
        }
        Group {
            name: "parser"
            prefix: qtscriptPath + "script/parser/"
            files: [
                "qscriptast.cpp",
                "qscriptastfwd_p.h",
                "qscriptast_p.h",
                "qscriptastvisitor.cpp",
                "qscriptastvisitor_p.h",
                "qscriptgrammar.cpp",
                "qscriptgrammar_p.h",
                "qscriptlexer.cpp",
                "qscriptlexer_p.h",
                "qscriptsyntaxchecker.cpp",
                "qscriptsyntaxchecker_p.h",
            ]
        }

        Export {
            Depends { name: "QtScriptFwdHeaders" }
            Depends { name: "cpp" }
            property stringList includePaths: [product.sourceDirectory + "/include"]
                                              .concat(QtScriptFwdHeaders.publicIncludePaths)
            Properties {
                condition: qbs.targetOS.contains("unix")
                cpp.dynamicLibraries: base.concat(["pthread"])
            }
            Properties {
                condition: qbs.targetOS.contains("windows")
                cpp.dynamicLibraries: base.concat(["winmm"])
            }
        }
    }
    Product {
        type: ["hpp"]
        name: "QtScriptFwdHeaders"
        Depends { name: "Qt.core" }
        Group {
            files: [
                "../../shared/qtscript/src/script/api/*.h"
            ]
            fileTags: ["qtscriptheader"]
        }
        Rule {
            multiplex: true
            inputs: ["qtscriptheader"]
            Artifact {
                filePath: "include/QtScript/qscriptengine.h"
                fileTags: ["hpp"]
            }
            prepare: {
                var syncQtPath = FileInfo.joinPaths(product.Qt.core.binPath, "syncqt.pl");
                if (!File.exists(syncQtPath)) {
                    // syncqt.pl is not in Qt's bin path. We might have a developer build.
                    // As we don't provide QT_HOST_BINS/src in our Qt modules we must
                    // kindly ask qmake.
                    var qmake = FileInfo.joinPaths(product.Qt.core.binPath,
                                                   "qmake" + product.cpp.executableSuffix);
                    var p = new Process();
                    if (p.exec(qmake, ["-query", "QT_HOST_BINS/src"]) !== 0)
                        throw new Error("Error while querying qmake.");
                    syncQtPath = FileInfo.joinPaths(p.readStdOut().replace(/\r?\n/, ''),
                                                    "syncqt.pl");
                }
                var qtScriptSrcPath = FileInfo.cleanPath(
                            FileInfo.path(inputs["qtscriptheader"][0].filePath) + "/../../..");
                console.info("qtScriptSrcPath: " + qtScriptSrcPath);
                var cmd = new Command("perl", [
                                          syncQtPath,
                                          "-minimal",
                                          "-version", product.Qt.core.version,
                                          "-outdir", FileInfo.cleanPath(
                                              FileInfo.path(output.filePath) + "/../.."),
                                          qtScriptSrcPath
                                      ]);
                cmd.description = "Create forwarding headers for the bundled QtScript module.";
                return cmd;
            }
        }
        Export {
            Depends { name: "Qt.core" }
            property stringList publicIncludePaths: [
                FileInfo.joinPaths(product.buildDirectory, "include")
            ]
            property stringList privateIncludePaths: [
                FileInfo.joinPaths(product.buildDirectory, "include",
                                   "QtScript", Qt.core.version, "QtScript")
            ]
        }
    }
}
