QtApplication {
    Depends { name: "Qt.core" }
    files: "clang-format.cpp"
    cpp.cxxLanguageVersion: "c++20"
    cpp.minimumMacosVersion: "11.0"
    cpp.warningLevel: "none"
}
