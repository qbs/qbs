/****************************************************************************
**
** Copyright (C) 2024 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

import qbs.TextFile
import qbs.Utilities

Product {
    condition: Utilities.versionCompare(Qt.core.version, "5.13") >= 0
    name: "lupdate-runner"
    type: "lupdate-result"
    builtByDefault: false

    property bool limitToSubProject: true
    property stringList extraArguments

    Depends { name: "Qt.core" }
    Depends { productTypes: "qm"; limitToSubProject: product.limitToSubProject }

    Rule {
        multiplex: true
        auxiliaryInputsFromDependencies: ["cpp", "objcpp", "qt.qml.qml", "qt.qml.js", "ui"]
        excludedInputs: "qt.untranslatable"
        Artifact {
            filePath: "lupdate.json"
            fileTags: "lupdate_json"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "creating lupdate project file";
            cmd.highlight = "filegen";
            cmd.sourceCode = function() {

                /*
                 * From the original commit message in qttools:
                 * Project ::= {
                 *   string projectFile     // Name of the project file.
                 *   string codec           // Source code codec. Valid values are
                 *                          // currently "utf-16" or "utf-8" (default).
                 *   string[] translations  // List of .ts files of the project.
                 *   string[] includePaths  // List of include paths.
                 *   string[] sources       // List of source files.
                 *   string[] excluded      // List of source files, which are
                 *                          // excluded for translation.
                 *   Project[] subProjects  // List of sub-projects.
                 * }
                 */
                var mainProject = {
                    projectFile: "main",
                    sources: [],
                    translations: [],
                    subProjects: []
                };

                var cxxArtifactsByIncludePaths = {};

                function handleProduct(product) {
                    (product.artifacts["ts"] || []).forEach(function(artifact) {
                        mainProject.translations.push(artifact.filePath);
                    });

                    ["qt.qml.qml", "qt.qml.js", "ui"].forEach(function(tag) {
                        (product.artifacts[tag] || []).forEach(function(artifact) {
                            mainProject.sources.push(artifact.filePath);
                        });
                    });

                    ["cpp", "objcpp"].forEach(function(tag) {
                        (product.artifacts[tag] || []).forEach(function(artifact) {
                            if (!artifact.cpp || artifact.fileTags.contains("qt.untranslatable"))
                                return;
                            var key = (artifact.cpp.includePaths || []).join("");
                            var current = cxxArtifactsByIncludePaths[key] || [];
                            current.push(artifact);
                            cxxArtifactsByIncludePaths[key] = current;
                        });
                    });
                };

                product.dependencies.forEach(function(dep) {
                    if (!dep.present)
                        handleProduct(dep);
                });

                var cxxProductNr = 0;
                for (key in cxxArtifactsByIncludePaths) {
                    var artifactList = cxxArtifactsByIncludePaths[key];
                    var cxxProject = {
                        projectFile: "cxx " + ++cxxProductNr,
                        includePaths: artifactList[0].cpp.includePaths,
                        sources: []
                    };
                    artifactList.forEach(function(artifact) {
                        cxxProject.sources.push(artifact.filePath);
                    });
                    mainProject.subProjects.push(cxxProject);
                }

                (new TextFile(output.filePath, TextFile.WriteOnly)).write(
                            JSON.stringify(mainProject));
            }
            return cmd;
        }
    }

    // This is a pure action target that intentionally does not do any change tracking.
    // It is implemented this way because it writes source files and therefore needs to be able
    // to overwrite user edits (e.g. accidental removal of a source string).
    Rule {
        inputs: "lupdate_json"
        outputFileTags: "lupdate-result"
        prepare: {
            var tool = product.Qt.core.binPath + '/' + product.Qt.core.lupdateName;
            var args = ["-project", inputs.lupdate_json[0].filePath]
                .concat(product.extraArguments || []);
            var cmd = new Command(tool, args);
            cmd.description = "updating translation files";
            return cmd;
        }
    }
}
