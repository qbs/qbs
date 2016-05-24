/****************************************************************************
 **
 ** Copyright (C) 2016 The Qt Company Ltd.
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

import com.sun.source.tree.ClassTree;
import com.sun.source.tree.MethodTree;
import com.sun.source.tree.VariableTree;
import com.sun.source.util.TreePathScanner;
import com.sun.source.util.Trees;

import javax.annotation.processing.ProcessingEnvironment;
import javax.lang.model.SourceVersion;
import javax.lang.model.element.*;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

public class ArtifactScanner extends TreePathScanner<Void, Trees> {
    private ProcessingEnvironment processingEnv;
    private SourceVersion sourceVersion;
    private Set<String> binaryNames = new HashSet<String>();
    private Set<String> nativeHeaderBinaryNames = new HashSet<String>();

    public ArtifactScanner(SourceVersion sourceVersion) {
        this.sourceVersion = sourceVersion;
    }

    public synchronized void init(ProcessingEnvironment processingEnv) {
        this.processingEnv = processingEnv;
    }

    public SourceVersion getSourceVersion() {
        return sourceVersion;
    }

    public Set<String> getBinaryNames() {
        return binaryNames;
    }

    public Set<String> getNativeHeaderBinaryNames() {
        return nativeHeaderBinaryNames;
    }

    private static TypeElement getEnclosingTypeElement(final Element element) {
        Element typeElement = element;
        while (!(typeElement instanceof TypeElement))
            typeElement = typeElement.getEnclosingElement();
        return (TypeElement) typeElement;
    }

    private static boolean isNativeHeaderFileGenerationSupported() {
        // This appears to depend on the actual JDK version, NOT the -source version given.
        return SourceVersion.latest().ordinal() >= 8;
    }

    @Override
    public Void visitClass(ClassTree node, Trees trees) {
        onVisitType((TypeElement)trees.getElement(getCurrentPath()));
        return super.visitClass(node, trees);
    }

    @Override
    public Void visitMethod(MethodTree node, Trees trees) {
        onVisitExecutable((ExecutableElement)trees.getElement(getCurrentPath()));
        return super.visitMethod(node, trees);
    }

    @Override
    public Void visitVariable(VariableTree node, Trees trees) {
        onVisitVariable((VariableElement)trees.getElement(getCurrentPath()));
        return super.visitVariable(node, trees);
    }

    public void onVisitType(TypeElement type) {
        binaryNames.add(processingEnv.getElementUtils().getBinaryName(type).toString());
    }

    public void onVisitExecutable(ExecutableElement element) {
        if (isNativeHeaderFileGenerationSupported() && element.getModifiers().contains(javax.lang.model.element.Modifier.NATIVE)) {
            nativeHeaderBinaryNames.add(processingEnv.getElementUtils().getBinaryName(getEnclosingTypeElement(element)).toString());
        }
    }

    public void onVisitVariable(VariableElement variable) {
        // Java 8: a native header is generated for a Java source file if it has either native methods,
        // or constant fields annotated with java.lang.annotation.Native (JDK-7150368).
        if (isNativeHeaderFileGenerationSupported()) {
            List<? extends AnnotationMirror> annotations = variable.getAnnotationMirrors();
            for (Iterator<? extends AnnotationMirror> it = annotations.iterator(); it.hasNext(); ) {
                // We could do variable.getAnnotation(java.lang.annotation.Native.class) != null,
                // but the helper tool must compile with older JDK versions.
                if (it.next().getAnnotationType().toString().equals("java.lang.annotation.Native"))
                    nativeHeaderBinaryNames.add(processingEnv.getElementUtils().getBinaryName(getEnclosingTypeElement(variable)).toString());
            }
        }
    }
}
