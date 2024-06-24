from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy
import os

class ConanModuleProviderTestlib(ConanFile):
    name = "conanmoduleprovider.testlibdep"
    license = "none"
    version = "1.2.3"

    exports_sources = "*.cpp", "*.h", "*.txt"
    settings = "os", "compiler", "build_type", "arch"

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
        self.cpp_info.libs = ['conanmoduleprovider.testlibdep']
        self.cpp_info.resdirs = ['share']

