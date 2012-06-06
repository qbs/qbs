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
#ifndef SCANNER_H
#define SCANNER_H


#ifdef __cplusplus
extern "C" {
#endif

#define SC_LOCAL_INCLUDE_FLAG   0x1
#define SC_GLOBAL_INCLUDE_FLAG  0x2

/**
  * Open a file that's going to be scanned.
  * The file path encoding is UTF-16 on all platforms.
  *
  * If the scanner is used for more than one type hint (e.g. C++ header / source)
  * the scanner can read the parameter fileTag which file type it is going to scan.
  *
  * Returns a scanner handle.
  */
typedef void *(*scanOpen_f)                 (const unsigned short *filePath, char **fileTags, int numFileTags);

/**
  * Closes the given scanner handle.
  */
typedef void  (*scanClose_f)                (void *opaq);

/**
  * Return the next result (filename) of the scan.
  */
typedef const char *(*scanNext_f)           (void *opaq, int *size, int *flags);

/**
  * Returns a list of type hints for the scanned file.
  * May return null.
  *
  * Example: if a C++ header file contains Q_OBJECT,
  * the type hint 'moc_hpp' is returned.
  */
typedef const char** (*scanAdditionalFileTags_f) (void *opaq, int *size);

struct ScannerPlugin
{
    const char  *name;
    const char  *fileTag;
    scanOpen_f  open;
    scanClose_f close;
    scanNext_f  next;
    scanAdditionalFileTags_f additionalFileTags;
    bool usesCppIncludePaths;
};

typedef ScannerPlugin **(*getScanners_f)();

#ifdef __cplusplus
}
#endif
#endif // SCANNER_H
