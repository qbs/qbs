from conan import ConanFile
from conan.tools.files import copy

import os

class Recipe(ConanFile):
    exports_sources = ("header.h")
    version = '0.1.0'
    name = 'conanmoduleprovider.testlibheader'

    def package(self):
        copy(self,
             "header.h",
             src=self.source_folder,
             dst=os.path.join(self.package_folder, "include"))