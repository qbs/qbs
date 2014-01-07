import qbs
import qbs.File
import qbs.TextFile

Product {
    type: {
        var original = new TextFile("original.txt", TextFile.WriteOnly);
        original.close();
        File.copy("original.txt", "copy.txt");
        var origTimestamp = File.lastModified("original.txt");
        var copyTimestamp = File.lastModified("copy.txt");
        if (origTimestamp > copyTimestamp)
            throw new Error("Older file has newer timestamp.");
        File.remove("original.txt");
        var copy = new TextFile("copy.txt", TextFile.WriteOnly);
        copy.writeLine(File.exists("original.txt"));
        copy.writeLine(File.exists("copy.txt"));
        copy.close();
        return "palim-palim";
    }
}
