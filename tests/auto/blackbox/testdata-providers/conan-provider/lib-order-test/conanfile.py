from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy
import os

class LibAConan(ConanFile):
    name = "conanmoduleprovider.lib-order-test"
    version = "1.0.0"
    license = "none"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = "CMakeLists.txt", "A/*", "B/*", "C/*"
    build_policy = "missing"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self, generator="Ninja")
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        # Component C
        self.cpp_info.components["C"].includedirs = []
        self.cpp_info.components["C"].libs = ["C"]

        # Component B depends on C
        self.cpp_info.components["B"].includedirs = []
        self.cpp_info.components["B"].libs = ["B"]
        self.cpp_info.components["B"].requires = ["C"]

        # Component A depends on B
        self.cpp_info.components["A"].includedirs = []
        self.cpp_info.components["A"].libs = ["A"]
        self.cpp_info.components["A"].requires = ["B"]

