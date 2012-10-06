/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#ifndef AUTOMOC_H
#define AUTOMOC_H

#include "buildgraph.h"

struct ScannerPlugin;

namespace qbs {

class ScanResultCache;

/**
  * Scans cpp and hpp files for the Q_OBJECT / Q_GADGET macro and
  * applies the corresponding rule then.
  * Also scans the files for moc_XXX.cpp files to find out if we must
  * compile and link a moc_XXX.cpp file or not.
  *
  * This whole thing is an ugly hack, I know.
  */
class AutoMoc
{
public:
    AutoMoc();

    void setScanResultCache(ScanResultCache *scanResultCache);
    void apply(BuildProduct::Ptr product);

private:
    enum FileType
    {
        UnknownFileType,
        HppFileType,
        CppFileType
    };

private:
    static QString generateMocFileName(Artifact *artifact, FileType fileType);
    static FileType fileType(Artifact *artifact);
    void scan(Artifact *artifact, bool &hasQObjectMacro, QSet<QString> &includedMocCppFiles);
    bool isVictimOfMoc(Artifact *artifact, FileType fileType, QString &foundMocFileTag);
    void unmoc(Artifact *artifact, const QString &mocFileTag);
    QList<ScannerPlugin *> scanners() const;

    mutable QList<ScannerPlugin *> m_scanners;
    ScanResultCache *m_scanResultCache;
};

} // namespace qbs

#endif // AUTOMOC_H
