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

package io.qt.qbs;

import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import java.util.List;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;

import org.w3c.dom.Document;
import org.w3c.dom.Element;

public class ArtifactListXmlWriter implements ArtifactListWriter {
    @Override
    public void write(List<Artifact> artifacts, OutputStream outputStream)
            throws IOException {
        try {
            DocumentBuilderFactory documentBuilderFactory = DocumentBuilderFactory
                    .newInstance();
            DocumentBuilder documentBuilder = documentBuilderFactory
                    .newDocumentBuilder();

            Document document = documentBuilder.newDocument();
            Element rootElement = document.createElement("artifacts");
            document.appendChild(rootElement);

            for (Artifact artifact : artifacts) {
                Element artifactElement = document.createElement("artifact");
                rootElement.appendChild(artifactElement);

                Element filePathElement = document.createElement("filePath");
                artifactElement.appendChild(filePathElement);
                filePathElement.appendChild(document.createTextNode(artifact
                        .getFilePath()));

                Element fileTagsElement = document.createElement("fileTags");
                artifactElement.appendChild(fileTagsElement);

                for (String fileTag : artifact.getFileTags()) {
                    Element fileTagElement = document.createElement("fileTag");
                    fileTagsElement.appendChild(fileTagElement);
                    fileTagElement
                            .appendChild(document.createTextNode(fileTag));
                }
            }

            TransformerFactory transformerFactory = TransformerFactory
                    .newInstance();
            Transformer transformer = transformerFactory.newTransformer();
            DOMSource source = new DOMSource(document);
            StreamResult result = new StreamResult(outputStream);

            transformer.transform(source, result);
            new PrintStream(outputStream).println();
        } catch (ParserConfigurationException pce) {
            throw new IOException(pce);
        } catch (TransformerException tfe) {
            throw new IOException(tfe);
        }
    }
}
