import qbs.Probes

Project {
    property string name: 'pkgconfig'
    CppApplication {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
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
