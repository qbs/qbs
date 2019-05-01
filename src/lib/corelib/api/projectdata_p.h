/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
#ifndef QBS_PROJECTDATA_P_H
#define QBS_PROJECTDATA_P_H

#include "projectdata.h"
#include <language/filetags.h>

#include <QtCore/qshareddata.h>

namespace qbs {
namespace Internal {

class InstallDataPrivate : public QSharedData
{
public:
    QString installFilePath;
    QString installRoot;
    bool isValid = false;
    bool isInstallable = false;
};

class ArtifactDataPrivate : public QSharedData
{
public:
    QString filePath;
    QStringList fileTags;
    PropertyMap properties;
    InstallData installData;
    bool isValid = false;
    bool isGenerated = false;
    bool isTargetArtifact = false;
};

class GroupDataPrivate : public QSharedData
{
public:
    QString name;
    QString prefix;
    CodeLocation location;
    QList<ArtifactData> sourceArtifacts;
    QList<ArtifactData> sourceArtifactsFromWildcards;
    PropertyMap properties;
    bool isEnabled = false;
    bool isValid = false;
};

class InstallableFilePrivate: public QSharedData
{
public:
    QString sourceFilePath;
    QString targetFilePath;
    QStringList fileTags;
    bool isValid = false;
};

class ProductDataPrivate : public QSharedData
{
public:
    QStringList type;
    QStringList dependencies;
    QString name;
    QString targetName;
    QString version;
    QString multiplexConfigurationId;
    CodeLocation location;
    QString buildDirectory;
    QList<GroupData> groups;
    QVariantMap properties;
    PropertyMap moduleProperties;
    QList<ArtifactData> generatedArtifacts;
    bool isEnabled = false;
    bool isRunnable = false;
    bool isMultiplexed = false;
    bool isValid = false;
};

class ProjectDataPrivate : public QSharedData
{
public:
    QString name;
    CodeLocation location;
    bool enabled = false;
    bool isValid = false;
    QList<ProductData> products;
    QList<ProjectData> subProjects;
    QString buildDir;
};

inline bool isRunnableArtifact(const FileTags &fileTags, bool isBundle)
{
    return (fileTags.contains("application") && (!isBundle || fileTags.contains("bundle.content")))
            || fileTags.contains("bundle.application-executable")
            || fileTags.contains("android.apk")
            || fileTags.contains("msi");
}

} // namespace Internal
} // namespace qbs

#endif // Include guard.
