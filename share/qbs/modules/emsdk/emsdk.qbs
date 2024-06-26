import qbs.Probes
import "emsdk.js" as Emsdk

Module
{
    property string configuredInstallPath
    property string detectedInstallPath: emccPathProbe.found ? emccPathProbe.path : undefined

    Probes.BinaryProbe
    {
        id: emccPathProbe
        condition: !configuredInstallPath
        names: ["emcc"]
    }

    property var environment: emsdkEnvProbe.environment

    Probe
    {
        id: emsdkEnvProbe
        property string emccDir: configuredInstallPath || detectedInstallPath
        property string shellPath: product.qbs.shellPath
        property var environment

        condition: emccDir
        configure: {
            environment = Emsdk.getEnvironment(shellPath, emccDir);
            found = true;
        }
    }
}
