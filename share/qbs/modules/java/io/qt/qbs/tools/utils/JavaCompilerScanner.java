/****************************************************************************
 **
 ** Copyright (C) 2015 Jake Petroules.
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

package io.qt.qbs.tools.utils;

import io.qt.qbs.*;

import javax.tools.*;
import javax.tools.JavaCompiler.CompilationTask;
import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.util.*;

public class JavaCompilerScanner {
    private static final String humanReadableTextFormat = "human-readable-text";
    private static final String jsonFormat = "json";
    private static final String xmlFormat = "xml1";

    private Set<String> compilationOutputFilePaths = new HashSet<String>();
    private Set<String> parsedOutputFilePaths = new HashSet<String>();
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

    private void addArtifact(Artifact a) {
        for (int i = 0; i < this.artifacts.size(); ++i) {
            if (this.artifacts.get(i).getFilePath().equals(a.getFilePath())) {
                return;
            }
        }

        this.artifacts.add(a);
        this.parsedOutputFilePaths.add(a.getFilePath());
    }

    public int run(List<String> compilerArguments) {
        artifacts.clear();
        JavaCompiler compiler = ToolProvider.getSystemJavaCompiler();
        StandardJavaFileManager standardFileManager = compiler
                .getStandardFileManager(null, null, null);
        JavaFileManager fileManager = new ForwardingJavaFileManager<StandardJavaFileManager>(
                standardFileManager) {
            @Override
            public JavaFileObject getJavaFileForOutput(
                    JavaFileManager.Location location, String className,
                    JavaFileObject.Kind kind, FileObject sibling)
                    throws IOException {
                JavaFileObject o = super.getJavaFileForOutput(location,
                        className, kind, sibling);
                compilationOutputFilePaths.add(new File(o.toUri().getSchemeSpecificPart()).toString());
                return new NullFileObject(o);
            }

            @Override
            public FileObject getFileForOutput(
                    JavaFileManager.Location location, String packageName,
                    String relativeName, FileObject sibling) throws IOException {
                FileObject o = super.getFileForOutput(location, packageName,
                        relativeName, sibling);
                compilationOutputFilePaths.add(new File(o.toUri().getSchemeSpecificPart()).toString());
                return new NullFileObject(o);
            }
        };

        final JavaCompilerOptions options = JavaCompilerOptions
                .parse(compiler, standardFileManager, compilerArguments
                        .toArray(new String[compilerArguments.size()]));

        final ArtifactScanner scanner = new ArtifactScanner(options.getSourceVersion());
        final ArtifactProcessor processor = new ArtifactProcessor(scanner);

        final CompilationTask task = compiler.getTask(
                null,
                fileManager,
                null,
                options.getRecognizedOptions(),
                options.getClassNames(),
                standardFileManager.getJavaFileObjectsFromFiles(options
                        .getFiles()));
        task.setProcessors(Arrays.asList(processor));
        task.call(); // exit code is not relevant, we have to ignore compilation errors sometimes

        final Set<String> binaryNames = scanner.getBinaryNames();
        final Iterator<String> it = binaryNames.iterator();
        while (it.hasNext()) {
            final String className = it.next();
            Artifact a = new Artifact(options.getClassFilesDir().replace('/', File.separatorChar)
                    + File.separatorChar + className.replace('.', File.separatorChar) + ".class");
            a.addFileTag("java.class");
            addArtifact(a);
        }

        final Set<String> nativeHeaderBinaryNames = scanner.getNativeHeaderBinaryNames();
        final Iterator<String> it2 = nativeHeaderBinaryNames.iterator();
        while (it2.hasNext()) {
            final String className = it2.next();
            Artifact a = new Artifact(options.getHeaderFilesDir().replace('/', File.separatorChar)
                    + File.separatorChar + className.replace('.', '_').replace('$', '_') + ".h");
            a.addFileTag("hpp");
            addArtifact(a);
        }

        // We can't simply compare both lists for equality because if compilation stopped due to an error,
        // only a partial list of artifacts may have been generated so far. If the parsed outputs contains file
        // paths it should not, qbs' --check-outputs option should catch that.
        if (!parsedOutputFilePaths.containsAll(compilationOutputFilePaths)) {
            ArrayList<String> parsedOutputFilePathsArray = new ArrayList<String>(parsedOutputFilePaths);
            Collections.sort(parsedOutputFilePathsArray);
            ArrayList<String> compilationOutputFilePathsArray = new ArrayList<String>(compilationOutputFilePaths);
            Collections.sort(compilationOutputFilePathsArray);
            compilationOutputFilePaths.removeAll(parsedOutputFilePaths);
            ArrayList<String> differenceArray = new ArrayList<String>(compilationOutputFilePaths);
            Collections.sort(differenceArray);

            System.err.println("The set of output files determined by source code parsing:\n\n"
                    + join("\n", parsedOutputFilePathsArray) + "\n\n"
                    + "is missing some files from the list that would be produced by the compiler:\n\n"
                    + join("\n", compilationOutputFilePathsArray) + "\n\n"
                    + "The missing files are:\n\n"
                    + join("\n", differenceArray) + "\n\n"
                    + "Compilation will still continue, though a build error *might* appear later;\n"
                    + "please check the bug report at https://bugreports.qt.io/browse/QBS-1069\n");
        }

        return !parsedOutputFilePaths.isEmpty() ? 0 : 1;
    }

    public void write(OutputStream outputStream) throws IOException {
        getOutputFormatters().get(outputFormat).write(artifacts, outputStream);
    }

    // Java 8 has String.join but the helper tool must build with Java 6.
    private static String join(CharSequence delimiter, Iterable<? extends CharSequence> elements) {
        StringBuilder sb = new StringBuilder();
        for (Iterator<? extends CharSequence> it = elements.iterator(); it.hasNext(); ) {
            sb.append(it.next());
            sb.append(delimiter);
        }

        // Remove the last delimiter
        if (sb.length() > 0)
            sb.replace(sb.length() - delimiter.length(), sb.length(), "");

        return sb.toString();
    }
}
