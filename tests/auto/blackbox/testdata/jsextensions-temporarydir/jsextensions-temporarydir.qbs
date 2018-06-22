import qbs.File
import qbs.TemporaryDir

Product {
    targetName: {
        var dir;
        var dirPath;
        try {
            dir = new TemporaryDir();
            dirPath = dir.path();
            if (!dirPath)
                throw "path is empty";

            if (!dir.isValid())
                throw "dir is not valid";

            if (!File.exists(dirPath))
                throw "dir does not exist";
        } finally {
            if (!dir.remove())
                throw "could not remove";
        }

        if (File.exists(dirPath))
            throw "dir was not removed";
    }
}
