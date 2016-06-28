import qbs 1.0

Project {
    // no minimum versions are specified so the profile defaults will be used
    QtApplication {
        name: "unspecified"
        files: "main.cpp"
        consoleApplication: true

        Properties {
            condition: qbs.targetOS.contains("darwin")
            cpp.frameworks: "Foundation"
        }
    }

    // no minimum versions are specified, and explicitly set to undefined in
    // case the profile has set it
    QtApplication {
        name: "unspecified-forced"
        files: "main.cpp"
        consoleApplication: true
        cpp.minimumWindowsVersion: undefined
        cpp.minimumMacosVersion: undefined
        cpp.minimumIosVersion: undefined
        cpp.minimumAndroidVersion: undefined

        Properties {
            condition: qbs.targetOS.contains("darwin")
            cpp.frameworks: "Foundation"
        }
    }

    // a specific version of the operating systems is specified
    // when the application is run its output should confirm
    // that the given values took effect
    QtApplication {
        condition: qbs.targetOS.contains("windows") || qbs.targetOS.contains("macos")
        name: "specific"
        files: "main.cpp"
        consoleApplication: true

        Properties {
            condition: qbs.targetOS.contains("windows")
            cpp.minimumWindowsVersion: "6.0"
        }

        Properties {
            condition: qbs.targetOS.contains("macos")
            cpp.frameworks: "Foundation"
            cpp.minimumMacosVersion: "10.6"
        }
    }

    // non-existent versions of Windows should print a QBS warning
    // (but will still compile and link since we avoid passing a
    // bad value to the linker)
    QtApplication {
        condition: qbs.targetOS.contains("windows")
        name: "fakewindows"
        files: "main.cpp"
        consoleApplication: true
        cpp.minimumWindowsVersion: "5.3"
    }

    // just to make sure three-digit minimum versions work on macOS
    // this only affects the value of __MAC_OS_X_VERSION_MIN_REQUIRED,
    // not the actual LC_VERSION_MIN_MACOSX command which is limited to two
    QtApplication {
        condition: qbs.targetOS.contains("macos")
        name: "macappstore"
        files: "main.cpp"
        consoleApplication: true
        cpp.frameworks: "Foundation"
        cpp.minimumMacosVersion: "10.6.8"
    }
}
