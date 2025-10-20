from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
import os

class ProtoLibConan(ConanFile):
    name = "protolib"
    license = "MIT"
    author = "iahve1991"
    url = "https://github.com/iahve-space/protolib/"
    description = "Protocol handling library"
    topics = ("protocol", "aarch64", "linux", "embedded")

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False]}
    default_options = {"shared": False}

    exports_sources = (
        "CMakeLists.txt", "install.cmake",
        "libraries/**", "protocols/**", "include/**",
        "cmake/**", ".project/**",
    )

    def set_version(self):
        for env_var in ("RELEASE_VERSION", "SEMVER", "CI_COMMIT_TAG"):
            v = os.getenv(env_var)
            if v:
                self.version = v.lstrip("v")
                break

    def layout(self):
        cmake_layout(self, src_folder=".")

    def generate(self):
        tc = CMakeToolchain(self)
        # tc.variables["BUILD_TESTING"] = "OFF"
        # tc.variables["INSTALL_GTEST"] = "OFF"
        # tc.variables["BUILD_GTEST"] = "OFF"
        # tc.variables["BUILD_GMOCK"] = "OFF"
        # tc.variables["BUILD_PACKAGE"] = "ON"
        # tc.variables["SOFTWARE_VERSION"] = str(self.version or "")
        tc.generate()
        self.output.info(f"[protolib] generate(): SOFTWARE_VERSION={self.version}")

    def build(self):
        cmake = CMake(self)
        cmake.configure(variables={
            "BUILD_TESTING": "OFF",
            "BUILD_CONAN": "OFF",
            "BUILD_PACKAGE": "ON",
            "SOFTWARE_VERSION": str(self.version or ""),
        })
        cmake.build()

    def package(self):
        CMake(self).install()

    def package_info(self):
        # Базовые пути
        self.cpp_info.set_property("cmake_file_name", "protolib")
        self.cpp_info.set_property("cmake_find_mode", "config")
        self.cpp_info.includedirs = ["include"]
        self.cpp_info.libdirs = ["lib"]
        self.cpp_info.builddirs.append(f"lib/cmake/protolib-{self.version}")

        # self.cpp_info.libs = [
        #     "protolib_interfaces",
        #     "protolib_crc_soft",
        #     "protolib_crc16_modbus",
        #     "protolib_lacte",
        # ]

        c = self.cpp_info.components

        # header-only
        c["fields"].set_property("cmake_target_name", "protolib::fields")
        c["fields"].libs = []

        c["containers"].set_property("cmake_target_name", "protolib::containers")
        c["containers"].libs = []

        c["interfaces"].set_property("cmake_target_name", "protolib::interfaces")
        c["interfaces"].libs = ["protolib_interfaces"]

        c["crc_soft"].set_property("cmake_target_name", "protolib::crc_soft")
        c["crc_soft"].libs = ["protolib_crc_soft"]

        c["crc16_modbus"].set_property("cmake_target_name", "protolib::crc16_modbus")
        c["crc16_modbus"].libs = ["protolib_crc16_modbus"]

        # c["lacte"].set_property("cmake_target_name", "protolib::lacte")
        # c["lacte"].libs = ["protolib_lacte"]
        # c["lacte"].requires = ["interfaces", "crc_soft", "crc16_modbus", "fields"]

        # зонтик — НО НЕ НАЗЫВАЙ его "protolib"
        c["all"].set_property("cmake_target_name", "protolib::all")
        c["all"].requires = ["fields", "interfaces", "crc_soft", "crc16_modbus", "containers"]