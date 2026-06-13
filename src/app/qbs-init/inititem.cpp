/****************************************************************************
**
** Copyright (C) 2026 Ivan Komissarov (abbapoh@gmail.com).
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "inititem.h"

#include <logging/translator.h>
#include <tools/error.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qtextstream.h>

namespace qbs {
using namespace Internal;

static QString itemTypeDirName(InitItem::ItemType itemType)
{
    switch (itemType) {
    case InitItem::ItemType::Application:
        return QStringLiteral("application");
    }
    Q_UNREACHABLE();
}

static QString languageDirName(InitItem::Language language)
{
    switch (language) {
    case InitItem::Language::C:
        return QStringLiteral("c");
    case InitItem::Language::Cpp:
        return QStringLiteral("cpp");
    case InitItem::Language::Java:
        return QStringLiteral("java");
    case InitItem::Language::ObjectiveC:
        return QStringLiteral("objc");
    }
    Q_UNREACHABLE();
}

static QString readFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        throw ErrorInfo(Tr::tr("Cannot read template file '%1'.").arg(filePath));
    return QString::fromUtf8(file.readAll());
}

static void writeFile(const QString &filePath, const QString &contents)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        throw ErrorInfo(Tr::tr("Cannot write file '%1'.").arg(filePath));
    QTextStream stream(&file);
    stream << contents;
}

static void ensureProjectDirectoryCanBeCreated(const QString &projectDirectoryPath)
{
    QFileInfo fi(projectDirectoryPath);
    if (fi.exists())
        throw ErrorInfo(
            Tr::tr("Directory '%1' already exists, aborting.").arg(projectDirectoryPath));
    if (!QDir().mkpath(projectDirectoryPath))
        throw ErrorInfo(Tr::tr("Cannot create directory '%1'.").arg(projectDirectoryPath));
}

static QString applyTemplate(QString contents, const QString &productName, const QString &version)
{
    return contents.replace(QLatin1String("@PRODUCT_NAME@"), productName)
        .replace(QLatin1String("@PRODUCT_VERSION@"), version);
}

void InitItem::run(
    const QString &directoryPath,
    const QString &projectName,
    const QString &version,
    ItemType itemType,
    Language language)
{
    const QString projectDirectoryPath = QDir(directoryPath).filePath(projectName);
    ensureProjectDirectoryCanBeCreated(projectDirectoryPath);

    const QString templateDirPath = QStringLiteral(":/init-templates/%1/%2")
                                        .arg(itemTypeDirName(itemType), languageDirName(language));
    const QDir templateDir(templateDirPath);
    if (!templateDir.exists()) {
        throw ErrorInfo(Tr::tr("No template found for item type '%1' and language '%2'.")
                            .arg(itemTypeDirName(itemType), languageDirName(language)));
    }

    const QStringList entries = templateDir.entryList(QDir::Files);
    if (entries.isEmpty()) {
        throw ErrorInfo(Tr::tr("Template directory '%1' is empty.").arg(templateDirPath));
    }

    for (const QString &entry : entries) {
        const QString contents = applyTemplate(
            readFile(templateDir.filePath(entry)), projectName, version);
        const QString outputFileName = applyTemplate(entry, projectName, version);
        const QString outputFilePath = QDir(projectDirectoryPath).filePath(outputFileName);
        writeFile(outputFilePath, contents);
    }
}

} // namespace qbs
