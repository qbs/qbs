/****************************************************************************
 **
 ** Copyright (C) 2015 Jake Petroules.
 ** Contact: http://www.qt.io/licensing
 **
 ** This file is part of the Qt Build Suite.
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

package io.qt.qbs.tools.utils;

import io.qt.qbs.Artifact;
import io.qt.qbs.ArtifactListJsonWriter;
import io.qt.qbs.ArtifactListTextWriter;
import io.qt.qbs.ArtifactListWriter;
import io.qt.qbs.ArtifactListXmlWriter;

import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.tools.FileObject;
import javax.tools.ForwardingJavaFileManager;
import javax.tools.JavaCompiler;
import javax.tools.JavaFileManager;
import javax.tools.JavaFileObject;
import javax.tools.StandardJavaFileManager;
import javax.tools.ToolProvider;

public class JavaCompilerScanner {
    private static final String humanReadableTextFormat = "human-readable-text";
    private static final String jsonFormat = "json";
    private static final String xmlFormat = "xml1";

    private List<Artifact> artifacts = new ArrayList<Artifact>();
    private String outputFormat = humanReadableTextFormat;

    private static Map<String, ArtifactListWriter> getOutputFormatters() {
        Map<String, ArtifactListWriter> outputFormatters = new HashMap<String, ArtifactListWriter>();
        outputFormatters.put(humanReadableTextFormat,
                new ArtifactListTextWriter());
        outputFormatters.put(jsonFormat, new ArtifactListJsonWriter());
        outputFormatters.put(xmlFormat, new ArtifactListXmlWriter());
        return outputFormatters;
    }

    public String getOutputFormat() {
        return this.outputFormat;
    }

    public void setOutputFormat(String outputFormat) {
        this.outputFormat = outputFormat;
    }

    public List<Artifact> getArtifacts() {
        return this.artifacts;
    }

    public int run(List<String> compilerArguments) {
        artifacts.clear();
        JavaCompiler compiler = ToolProvider.getSystemJavaCompiler();
        StandardJavaFileManager standardFileManager = compiler
                .getStandardFileManager(null, null, null);
        JavaFileManager fileManager = new ForwardingJavaFileManager<StandardJavaFileManager>(
                standardFileManager) {
            public JavaFileObject getJavaFileForOutput(
                    JavaFileManager.Location location, String className,
                    JavaFileObject.Kind kind, FileObject sibling)
                    throws IOException {
                JavaFileObject o = super.getJavaFileForOutput(location,
                        className, kind, sibling);
                Artifact artifact = new Artifact(new File(o.toUri()
                        .getSchemeSpecificPart()).toString());
                if (kind.equals(JavaFileObject.Kind.CLASS)) {
                    artifact.addFileTag("java.class");
                } else if (kind.equals(JavaFileObject.Kind.SOURCE)) {
                    artifact.addFileTag("java.java");
                }
                artifacts.add(artifact);
                return new NullFileObject(o);
            }

            public FileObject getFileForOutput(
                    JavaFileManager.Location location, String packageName,
                    String relativeName, FileObject sibling) throws IOException {
                FileObject o = super.getFileForOutput(location, packageName,
                        relativeName, sibling);
                Artifact artifact = new Artifact(new File(o.toUri()
                        .getSchemeSpecificPart()).toString());
                if (o.getName().endsWith(".h")) {
                    artifact.addFileTag("hpp");
                }
                artifacts.add(artifact);
                return new NullFileObject(o);
            }
        };

        final JavaCompilerOptions options = JavaCompilerOptions
                .parse(compiler, standardFileManager, compilerArguments
                        .toArray(new String[compilerArguments.size()]));
        return compiler.getTask(
                null,
                fileManager,
                null,
                options.getRecognizedOptions(),
                options.getClassNames(),
                standardFileManager.getJavaFileObjectsFromFiles(options
                        .getFiles())).call() ? 0 : 1;
    }

    public void write(OutputStream outputStream) throws IOException {
        getOutputFormatters().get(outputFormat).write(artifacts, outputStream);
    }
}
