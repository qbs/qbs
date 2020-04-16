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

#ifndef GENERATORDATA_H
#define GENERATORDATA_H

#include <QtCore/qdir.h>
#include <QtCore/qmap.h>
#include <api/project.h>
#include <api/projectdata.h>
#include <tools/error.h>
#include <tools/installoptions.h>
#include <functional>

namespace qbs {

using GeneratableProjectMap = QMap<QString, Project>;

template <typename U> struct IMultiplexableContainer {
    QMap<QString, U> data;

    template <typename T> T uniqueValue(const std::function<T(const U &data)> &func,
                                        const QString &errorMessage) const
    {
        if (data.empty())
            return T();
        auto it = data.begin(), end = data.end();
        auto value = func(*it++);
        for (; it != end; ++it) {
            if (value != func(*it))
                throw ErrorInfo(errorMessage);
        }
        return value;
    }

    void forEach(const std::function<void(const QString &configurationName,
                                          const U &data)> &func) const
    {
        for (auto it = data.cbegin(), end = data.cend(); it != end; ++it)
            func(it.key(), it.value());
    }

    void forEach(const std::function<void(const std::string &configurationName,
                                          const U &data)> &func) const
    {
        for (auto it = data.cbegin(), end = data.cend(); it != end; ++it)
            func(it.key().toStdString(), it.value());
    }

    const U operator[](const QString &configurationName) const
    {
        return data[configurationName];
    }

    const U operator[](const std::string &configurationName) const
    {
        return data[QString::fromStdString(configurationName)];
    }

    bool isValid() const
    {
        return !data.empty();
    }

protected:
    IMultiplexableContainer() = default;
};

struct QBS_EXPORT GeneratableProductData : public IMultiplexableContainer<ProductData> {
    QString name() const;
    CodeLocation location() const;
    QStringList dependencies() const;
    QStringList type() const;
    QString buildDirectory() const;
};

struct QBS_EXPORT GeneratableProjectData : public IMultiplexableContainer<ProjectData> {
    struct Id {
    private:
        friend struct GeneratableProjectData;
        Id() = default;
        QString value;

    public:
        bool operator<(const Id &id) const { return value < id.value; }
    };

    QList<GeneratableProjectData> subProjects;
    QList<GeneratableProductData> products;
    QString name() const;
    CodeLocation location() const;
    Id uniqueName() const;
};

struct QBS_EXPORT GeneratableProject : public GeneratableProjectData {
    GeneratableProjectMap projects;
    QMap<QString, QVariantMap> buildConfigurations;
    QMap<QString, QStringList> commandLines;
    InstallOptions installOptions;
    QDir baseBuildDirectory() const;
    QFileInfo filePath() const;
    bool hasMultipleConfigurations() const;
    QStringList commandLine() const;

    void forEach(const std::function<void(const QString &configurationName,
                                          const Project &data)> &func) const
    {
        for (auto it = projects.cbegin(), end = projects.cend(); it != end; ++it)
            func(it.key(), it.value());
    }

    void forEach(const std::function<void(const std::string &configurationName,
                                          const Project &data)> &func) const
    {
        for (auto it = projects.cbegin(), end = projects.cend(); it != end; ++it)
            func(it.key().toStdString(), it.value());
    }

    const Project operator[](const QString &configurationName) const
    {
        return projects[configurationName];
    }

    const Project operator[](const std::string &configurationName) const
    {
        return projects[QString::fromStdString(configurationName)];
    }

    bool isValid() const
    {
        return !data.empty() && !projects.empty();
    }

    const ProjectData projectData(const QString &configurationName) const
    {
        return data[configurationName];
    }

    const ProjectData projectData(const std::string &configurationName) const
    {
        return data[QString::fromStdString(configurationName)];
    }
};

} // namespace qbs

#endif // GENERATORDATA_H
