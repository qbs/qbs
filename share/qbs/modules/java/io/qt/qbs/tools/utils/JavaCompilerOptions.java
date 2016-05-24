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

import javax.lang.model.SourceVersion;
import javax.tools.JavaCompiler;
import javax.tools.StandardJavaFileManager;
import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class JavaCompilerOptions {
    private final List<String> recognizedOptions;
    private final List<String> classNames;
    private final List<File> files;
    private String classFilesDir;
    private String headerFilesDir;
    private SourceVersion sourceVersion;

    public String getClassFilesDir() {
        return classFilesDir;
    }

    public String getHeaderFilesDir() {
        return headerFilesDir;
    }

    private JavaCompilerOptions(List<String> recognizedOptions, List<String> classNames, List<File> files,
                                String classFilesDir, String headerFilesDir, SourceVersion sourceVersion) {
        this.recognizedOptions = recognizedOptions;
        this.classNames = classNames;
        this.files = files;
        this.classFilesDir = classFilesDir;
        this.headerFilesDir = headerFilesDir;
        this.sourceVersion = sourceVersion;
    }

    public static JavaCompilerOptions parse(JavaCompiler compiler,
            StandardJavaFileManager fileManager, String... arguments) {
        List<String> recognizedOptions = new ArrayList<String>();
        List<String> classNames = new ArrayList<String>();
        List<File> files = new ArrayList<File>();
        String classFilesDir = null;
        String headerFilesDir = null;
        String sourceVersionString = null;
        int sourceVersion = SourceVersion.latest().ordinal();

        for (int i = 0; i < arguments.length; ++i) {
            int argumentCount = compiler.isSupportedOption(arguments[i]);
            if (argumentCount < 0)
                argumentCount = fileManager.isSupportedOption(arguments[i]);

            if (argumentCount >= 0) {
                if (arguments[i].equals("-d") && argumentCount == 1)
                    classFilesDir = arguments[i + 1];

                if (arguments[i].equals("-h") && argumentCount == 1)
                    headerFilesDir = arguments[i + 1];

                if (arguments[i].equals("-source") && argumentCount == 1) {
                    String a = arguments[i + 1];
                    if (a.equals("1.0"))
                        sourceVersion = 0;
                    else if (a.equals("1.1"))
                        sourceVersion = 1;
                    else if (a.equals("1.2"))
                        sourceVersion = 2;
                    else if (a.equals("1.3"))
                        sourceVersion = 3;
                    else if (a.equals("1.4"))
                        sourceVersion = 4;
                    else if (a.equals("1.5") || a.equals("5"))
                        sourceVersion = 5;
                    else if (a.equals("1.6") || a.equals("6"))
                        sourceVersion = 6;
                    else if (a.equals("1.7") || a.equals("7"))
                        sourceVersion = 7;
                    else if (a.equals("1.8") || a.equals("8"))
                        sourceVersion = 8;
                    sourceVersionString = a;
                }

                for (int j = 0; j < argumentCount + 1; ++j) {
                    if (i + j >= arguments.length) {
                        throw new IllegalArgumentException(arguments[i]);
                    }

                    recognizedOptions.add(arguments[i + j]);
                }

                i += argumentCount;
            } else {
                File file = new File(arguments[i]);
                if (file.exists())
                    files.add(file);
                else if (SourceVersion.isName(arguments[i]))
                    classNames.add(arguments[i]);
                else
                    throw new IllegalArgumentException(arguments[i]);
            }
        }

        if (sourceVersion >= SourceVersion.values().length)
            throw new RuntimeException("invalid source release: " + sourceVersionString);

        return new JavaCompilerOptions(recognizedOptions, classNames, files, classFilesDir, headerFilesDir,
                SourceVersion.values()[sourceVersion]);
    }

    public List<String> getRecognizedOptions() {
        return Collections.unmodifiableList(recognizedOptions);
    }

    public List<File> getFiles() {
        return Collections.unmodifiableList(files);
    }

    public List<String> getClassNames() {
        return Collections.unmodifiableList(classNames);
    }

    public SourceVersion getSourceVersion() {
        return sourceVersion;
    }
}
