import qbs.Host
import qbs.Probes

Project {
    property string name: 'pkgconfig'
    CppApplication {
        condition: {
            var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
            if (!result)
                console.info("target platform/arch differ from host platform/arch ("
                             + qbs.targetPlatform + "/" + qbs.architecture + " vs "
                             + Host.platform() + "/" + Host.architecture() + ")");
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
