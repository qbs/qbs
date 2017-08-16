import qbs 1.0
import qbs.Probes

Project {
    property string name: 'pkgconfig'
    CppApplication {
        name: project.name
        Probes.PkgConfigProbe {
            id: pkgConfig
            name: "QtCore"
            minVersion: '4.0.0'
            maxVersion: '5.99.99'
        }
        files: 'main.cpp'
        cpp.cxxFlags: pkgConfig.cflags
        cpp.linkerFlags: pkgConfig.libs
    }
}
