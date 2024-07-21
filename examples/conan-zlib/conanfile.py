from conan import ConanFile
from conan.tools.qbs import Qbs, QbsProfile, QbsDeps

class Recipe(ConanFile):
    name = "conan-zlib"
    version = "1.0"

    exports_sources = "*.c", "*.qbs",
    settings = "os", "compiler", "arch", "build_type"

    requires = ["zlib/1.3.1"]

    def generate(self):
        profile = QbsProfile(self)
        profile.generate()
        deps = QbsDeps(self)
        deps.generate()

    def build(self):
        qbs = Qbs(self)
        qbs.resolve()
        qbs.build()

    def package(self):
        qbs = Qbs(self)
        qbs.install()
