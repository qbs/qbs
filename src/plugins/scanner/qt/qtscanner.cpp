/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#if defined(WIN32) || defined(_WIN32)
#define SCANNER_EXPORT __declspec(dllexport)
#else
#define SCANNER_EXPORT
#endif

#include "../scanner.h"

#include <QtGlobal>

#ifdef Q_OS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#include <QFile>
#endif

#include <QScopedPointer>
#include <QString>
#include <QXmlStreamReader>

struct Opaq
{
#ifdef Q_OS_UNIX
    int fd;
    int mapl;
#else
    QFile *file;
#endif

    char *map;
    QXmlStreamReader *xml;
    QByteArray current;
    Opaq()
#ifdef Q_OS_UNIX
        : fd (0),
#else
        : file(0),
#endif
          map(0),
          xml(0)
    {}

    ~Opaq()
    {
#ifdef Q_OS_UNIX
        if (map)
            munmap (map, mapl);
        if (fd)
            close (fd);
#else
        delete file;
#endif
        delete xml;
    }
};

static void *openScanner(const unsigned short *filePath, char **fileTags, int numFileTags)
{
    Q_UNUSED(fileTags);
    Q_UNUSED(numFileTags);
    QScopedPointer<Opaq> opaque(new Opaq);

#ifdef Q_OS_UNIX
    QString filePathS = QString::fromUtf16(filePath);
    opaque->fd = open(qPrintable(filePathS), O_RDONLY);
    if (opaque->fd == -1) {
        opaque->fd = 0;
        return 0;
    }

    struct stat s;
    int r = fstat(opaque->fd, &s);
    if (r != 0)
        return 0;
    opaque->mapl = s.st_size;

    void *map = mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, opaque->fd, 0);
    if (map == 0)
        return 0;
#else
    opaque->file = new QFile(QString::fromUtf16(filePath));
    if (!opaque->file->open(QFile::ReadOnly))
        return 0;

    uchar *map = opaque->file->map(0, opaque->file->size());
    if (!map)
        return 0;
#endif

    opaque->map = reinterpret_cast<char *>(map);
    opaque->xml = new QXmlStreamReader(opaque->map);

    return static_cast<void *>(opaque.take());
}

static void closeScanner(void *ptr)
{
    Opaq *opaque = static_cast<Opaq *>(ptr);
    delete opaque;
}

static const char *nextQrc(void *opaq, int *size, int *flags)
{
    Opaq *o= static_cast<Opaq *>(opaq);
    while (!o->xml->atEnd()) {
        o->xml->readNext();
        switch (o->xml->tokenType()) {
            case QXmlStreamReader::StartElement:
                if ( o->xml->name() == "file") {
                    o->current = o->xml->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement).toUtf8();
                    *flags = SC_LOCAL_INCLUDE_FLAG;
                    *size = o->current.size();
                    return o->current.data();
                }
                break;
            case QXmlStreamReader::EndDocument:
                return 0;
            default:
                break;
        }
    }
    return 0;
}

static const char **additionalFileTags(void *, int *size)
{
    *size = 0;
    return 0;
}

extern "C" {

ScannerPlugin qrcScanner =
{
    "qt_qrc_scanner",
    "qrc",
    openScanner,
    closeScanner,
    nextQrc,
    additionalFileTags,
    NoScannerFlags
};

ScannerPlugin *theScanners[] = {&qrcScanner, NULL};

SCANNER_EXPORT ScannerPlugin **getScanners()
{
    return theScanners;
}

} // extern "C"
