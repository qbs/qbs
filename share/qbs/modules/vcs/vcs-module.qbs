import qbs.File
import qbs.FileInfo
import qbs.Process
import qbs.TextFile
import qbs.Utilities

Module {
    property string type: typeProbe.type
    property string repoDir: project.sourceDirectory
    property string toolFilePath: {
        if (type === "git")
            return "git";
        if (type === "svn")
            return "svn";
    }

    property string headerFileName: "vcs-repo-state.h"
    readonly property string repoState: gitProbe.repoState || subversionProbe.repoState
    readonly property string repoLatestTag: gitProbe.repoLatestTag || subversionProbe.repoLatestTag
    readonly property string repoCommitsSinceTag: gitProbe.repoCommitsSinceTag || subversionProbe.repoCommitsSinceTag
    readonly property string repoCommitSha: gitProbe.repoCommitSha || subversionProbe.repoCommitSha

    // Internal
    readonly property string includeDir: FileInfo.joinPaths(product.buildDirectory, "vcs-include")
    readonly property string metaDataBaseDir: typeProbe.metaDataBaseDir

    PropertyOptions {
        name: "type"
        allowedValues: ["git", "svn"]
        description: "the version control system your project is using"
    }

    Group {
        condition: headerFileName
        Depends { name: "cpp" }
        product.cpp.includePaths: [includeDir]
        Rule {
            multiplex: true
            Artifact {
                filePath: FileInfo.joinPaths(product.vcs.includeDir, product.vcs.headerFileName)
                fileTags: ["hpp"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + output.fileName;
                cmd.highlight = "codegen";
                cmd.repoState = product.vcs.repoState;
                cmd.repoLatestTag = product.vcs.repoLatestTag;
                cmd.repoCommitsSinceTag = product.vcs.repoCommitsSinceTag;
                cmd.repoCommitSha = product.vcs.repoCommitSha;
                cmd.sourceCode = function() {
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    try {
                        f.writeLine("#ifndef VCS_REPO_STATE_H");
                        f.writeLine("#define VCS_REPO_STATE_H");
                        f.writeLine('#define VCS_REPO_STATE "' + (repoState ? repoState : "none") + '"')
                        f.writeLine('#define VCS_REPO_LATEST_TAG "' + (repoLatestTag ? repoLatestTag : "none") + '"')
                        f.writeLine('#define VCS_REPO_COMMITS_SINCE_TAG "' + (repoCommitsSinceTag ? repoCommitsSinceTag : "none") + '"')
                        f.writeLine('#define VCS_REPO_COMMIT_SHA "' + (repoCommitSha ? repoCommitSha : "none") + '"')
                        f.writeLine("#endif");
                    } finally {
                        f.close();
                    }
                };
                return [cmd];
            }
        }
    }

    Probe {
        id: typeProbe

        property string tool: toolFilePath
        property string theRepoDir: repoDir

        property string type
        property string metaDataBaseDir

        configure: {
            var detector = new Process();
            try {
                detector.setWorkingDirectory(theRepoDir);
                if (detector.exec(tool || "git", ["rev-parse", "--git-dir"]) === 0) {
                    found = true;
                    type = "git";
                    metaDataBaseDir = detector.readStdOut().trim();
                    if (!FileInfo.isAbsolutePath(metaDataBaseDir))
                        metaDataBaseDir = FileInfo.joinPaths(theRepoDir, metaDataBaseDir);
                    return;
                }
                if (detector.exec(tool || "svn",
                                  ["info", "--show-item", "wc-root", "--no-newline"]) === 0) {
                    found = true
                    type = "svn";
                    metaDataBaseDir = FileInfo.joinPaths(detector.readStdOut(), ".svn");
                    return;
                } else if (detector.exec(tool || "svn", ["info"]) === 0) {
                    if (detector.exec(tool || "svn", ["--version", "--quiet"]) === 0
                            && Utilities.versionCompare(detector.readStdOut().trim(), "1.9") < 0) {
                        throw "svn too old, version >= 1.9 required";
                    }
                }
            } finally {
                detector.close();
            }
        }
    }

    Probe {
        id: gitProbe
        condition: type === "git"

        property string tool: toolFilePath
        property string theRepoDir: repoDir
        property string filePath: FileInfo.joinPaths(metaDataBaseDir, "logs/HEAD")
        property var timestamp: File.lastModified(filePath)

        property string repoState
        property string repoLatestTag
        property string repoCommitsSinceTag
        property string repoCommitSha

        configure: {
            if (!File.exists(filePath)) {
                // it is possible that the HEAD file is not present in CI environments, in this
                // case we can use the commit count to determine the repo state
                // see https://bugreports.qt.io/projects/QBS/issues/QBS-1814
                try {
                    var proc = new Process();
                    proc.setWorkingDirectory(theRepoDir);
                    proc.exec(tool, ["rev-list", "HEAD", "--count"], true);
                } catch (e) {
                    return; // No commits yet.
                } finally {
                    proc.close();
                }
            }
            try {
                var proc = new Process();
                proc.setWorkingDirectory(theRepoDir);
                proc.exec(tool, ["describe", "--always", "--long", "HEAD"], true);
                repoState = proc.readStdOut().trim();
                if (repoState) {
                    found = true;

                    // tag is formatted as TAG-N-gSHA:
                    // 1. latest tag is TAG
                    // 2. number of commits since latest TAG is N
                    // 3. latest commit is gSHA
                    const tagSections = repoState.split("-").reverse();

                    if (tagSections.length >= 3) {
                        repoCommitSha = tagSections[0];
                        repoCommitsSinceTag = tagSections[1];
                        repoLatestTag = tagSections.slice(2).reverse().join("-"); // Handle tags with dashes
                    } else {
                        repoCommitSha = "g" + tagSections[0];
                    }
                }
            } finally {
                proc.close();
            }
        }
    }

    Probe {
        id: subversionProbe
        condition: type === "svn"

        property string tool: toolFilePath
        property string theRepoDir: repoDir
        property string filePath: FileInfo.joinPaths(metaDataBaseDir, "wc.db")
        property var timestamp: File.lastModified(filePath)

        property string repoState
        property string repoLatestTag
        property string repoCommitsSinceTag
        property string repoCommitSha

        configure: {
            var proc = new Process();
            try {
                proc.setWorkingDirectory(theRepoDir);
                proc.exec(tool, ["info", "-r", "HEAD", "--show-item", "revision", "--no-newline"],
                          true);
                repoState = proc.readStdOut().trim();
                if (repoState)
                    found = true;
            } finally {
                proc.close();
            }
        }
    }
}
