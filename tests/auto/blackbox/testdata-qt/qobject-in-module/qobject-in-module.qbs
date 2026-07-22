import qbs.Host

Project {
    DynamicLibrary {
        name: "lib"
        condition: {
            if (!qbs.toolchain.includes("clang-cl") && !qbs.toolchain.includes("xcode")
                    && (qbs.toolchain.includes("msvc")
                        || (qbs.toolchain.includes("clang") && cpp.compilerVersionMajor >= 16))) {
                return true;
            }
            console.info("Unsupported toolchain " + JSON.stringify(qbs.toolchain));
            return false;
        }
        Depends { name: "Qt.core" }
        property bool dummy: {
            const ok = qbs.targetPlatform === Host.platform()
                && (qbs.architecture === undefined || qbs.architecture === Host.architecture());

            // Weird error on Windows/arm CI machine:
            // "'D:/a/qbs/qbs/release/install-root/tests/auto/blackbox-qt/testWorkDir/qobject-in-module/default/install-root/bin/qobject-in-module.exe'
            // could not be started: Process failed to start: This version of %1 is not compatible
            // with the version of Windows you're running. Check your computer's system information
            // and then contact the software publisher."
            if (ok && Host.platform() === "windows" && Host.architecture() === "arm64")
                ok = false;

            if (!ok)
                console.info("target platform/arch differ from host platform/arch ("
                             + qbs.targetPlatform + "/" + qbs.architecture + " vs "
                             + Host.platform() + "/" + Host.architecture() + ")");
            return ok;
        }
        cpp.cxxLanguageVersion: "c++20"
        cpp.forceUseCxxModules: true
        files: "module.cppm"
    }
    Application {
        condition: lib.present
        Depends { name: "lib"; required: false }
        Depends { name: "Qt.core" }
        cpp.cxxLanguageVersion: "c++20"
        cpp.forceUseCxxModules: true
        files: "main.cpp"
    }
}
