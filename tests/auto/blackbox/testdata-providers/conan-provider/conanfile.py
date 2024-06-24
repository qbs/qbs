from conan import ConanFile

class Recipe(ConanFile):
    name = "qbstest"
    version = "1.0"

    exports_sources = "*.cpp", "*.h", "*.txt"
    settings = "os", "compiler", "build_type", "arch"

    requires = ["conanmoduleprovider.testlib/1.2.3", "conanmoduleprovider.testlibheader/0.1.0"]
