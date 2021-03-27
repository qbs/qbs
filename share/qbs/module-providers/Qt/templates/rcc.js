var Utilities = require("qbs.Utilities");

function fullPath(product)
{
    if (Utilities.versionCompare(product.Qt.core.version, "6.1") < 0)
        return product.Qt.core.binPath + '/' + product.Qt.core.rccName;
    return product.Qt.core.libExecPath + '/' + product.Qt.core.rccName;
}
