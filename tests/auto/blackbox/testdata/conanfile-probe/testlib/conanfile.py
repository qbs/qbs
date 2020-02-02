from conans import ConanFile

class Testlib(ConanFile):
    name = "testlib"
    description = "Represents an arbitrary package, for instance on bintray"
    license = "none"
    version = "1.2.3"

    settings = "os"
    options = {"opt": [True, False]}
    default_options = {"opt": False}

    def source(self):
        pass

    def build(self):
        pass

    def package(self):
        pass

    def package_info(self):
        self.cpp_info.libs = ["testlib1","testlib2"]
        self.env_info.ENV_VAR = "TESTLIB_ENV_VAL"
        self.user_info.user_var = "testlib_user_val"
