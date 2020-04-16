/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#include "session.h"

#include "sessionpacket.h"
#include "sessionpacketreader.h"

#include <api/jobs.h>
#include <api/project.h>
#include <api/projectdata.h>
#include <api/runenvironment.h>
#include <logging/ilogsink.h>
#include <tools/buildoptions.h>
#include <tools/cleanoptions.h>
#include <tools/error.h>
#include <tools/installoptions.h>
#include <tools/jsonhelper.h>
#include <tools/preferences.h>
#include <tools/processresult.h>
#include <tools/qbsassert.h>
#include <tools/settings.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qobject.h>
#include <QtCore/qprocess.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>

#ifdef Q_OS_WIN32
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <io.h>
#endif

namespace qbs {
namespace Internal {

class SessionLogSink : public QObject, public ILogSink
{
    Q_OBJECT
signals:
    void newMessage(const QJsonObject &msg);

private:
    void doPrintMessage(LoggerLevel, const QString &message, const QString &) override
    {
        QJsonObject msg;
        msg.insert(StringConstants::type(), QLatin1String("log-data"));
        msg.insert(StringConstants::messageKey(), message);
        emit newMessage(msg);
    }

    void doPrintWarning(const ErrorInfo &warning) override
    {
        QJsonObject msg;
        static const QString warningString(QLatin1String("warning"));
        msg.insert(StringConstants::type(), warningString);
        msg.insert(warningString, warning.toJson());
        emit newMessage(msg);
    }
};

class Session : public QObject
{
    Q_OBJECT
public:
    Session();

private:
    enum class ProjectDataMode { Never, Always, OnlyIfChanged };
    ProjectDataMode dataModeFromRequest(const QJsonObject &request);
    QStringList modulePropertiesFromRequest(const QJsonObject &request);
    void insertProjectDataIfNecessary(
            QJsonObject &reply,
            ProjectDataMode dataMode,
            const ProjectData &oldProjectData,
            bool includeTopLevelData
            );
    void setLogLevelFromRequest(const QJsonObject &request);
    bool checkNormalRequestPrerequisites(const char *replyType);

    void sendPacket(const QJsonObject &message);
    void setupProject(const QJsonObject &request);
    void buildProject(const QJsonObject &request);
    void cleanProject(const QJsonObject &request);
    void installProject(const QJsonObject &request);
    void addFiles(const QJsonObject &request);
    void removeFiles(const QJsonObject &request);
    void getRunEnvironment(const QJsonObject &request);
    void getGeneratedFilesForSources(const QJsonObject &request);
    void releaseProject();
    void cancelCurrentJob();
    void quitSession();

    void sendErrorReply(const char *replyType, const QString &message);
    void sendErrorReply(const char *replyType, const ErrorInfo &error);
    void insertErrorInfoIfNecessary(QJsonObject &reply, const ErrorInfo &error);
    void connectProgressSignals(AbstractJob *job);
    QList<ProductData> getProductsByName(const QStringList &productNames) const;
    ProductData getProductByName(const QString &productName) const;

    struct ProductSelection {
        ProductSelection(Project::ProductSelection s) : selection(s) {}
        ProductSelection(QList<ProductData> p) : products(std::move(p)) {}

        Project::ProductSelection selection = Project::ProductSelectionDefaultOnly;
        QList<ProductData> products;
    };
    ProductSelection getProductSelection(const QJsonObject &request);

    struct FileUpdateData {
        QJsonObject createErrorReply(const char *type, const QString &mainMessage) const;

        ProductData product;
        GroupData group;
        QStringList filePaths;
        ErrorInfo error;
    };
    FileUpdateData prepareFileUpdate(const QJsonObject &request);

    SessionPacketReader m_packetReader;
    Project m_project;
    ProjectData m_projectData;
    SessionLogSink m_logSink;
    std::unique_ptr<Settings> m_settings;
    QJsonObject m_resolveRequest;
    QStringList m_moduleProperties;
    AbstractJob *m_currentJob = nullptr;
};

void startSession()
{
    const auto session = new Session;
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, session, [session] { delete session; });
}

Session::Session()
{
#ifdef Q_OS_WIN32
    // Make sure the line feed character appears as itself.
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        constexpr size_t errmsglen = FILENAME_MAX;
        char errmsg[errmsglen];
        strerror_s(errmsg, errmsglen, errno);
        std::cerr << "Failed to set stdout to binary mode: " << errmsg << std::endl;
        qApp->exit(EXIT_FAILURE);
    }
#endif
    sendPacket(SessionPacket::helloMessage());
    connect(&m_logSink, &SessionLogSink::newMessage, this, &Session::sendPacket);
    connect(&m_packetReader, &SessionPacketReader::errorOccurred,
            this, [](const QString &msg) {
        std::cerr << qPrintable(tr("Error: %1").arg(msg));
        qApp->exit(EXIT_FAILURE);
    });
    connect(&m_packetReader, &SessionPacketReader::packetReceived, this, [this](const QJsonObject &packet) {
        // qDebug() << "got packet:" << packet; // Uncomment for debugging.
        const QString type = packet.value(StringConstants::type()).toString();
        if (type == QLatin1String("resolve-project"))
            setupProject(packet);
        else if (type == QLatin1String("build-project"))
            buildProject(packet);
        else if (type == QLatin1String("clean-project"))
            cleanProject(packet);
        else if (type == QLatin1String("install-project"))
            installProject(packet);
        else if (type == QLatin1String("add-files"))
            addFiles(packet);
        else if (type == QLatin1String("remove-files"))
            removeFiles(packet);
        else if (type == QLatin1String("get-run-environment"))
            getRunEnvironment(packet);
        else if (type == QLatin1String("get-generated-files-for-sources"))
            getGeneratedFilesForSources(packet);
        else if (type == QLatin1String("release-project"))
            releaseProject();
        else if (type == QLatin1String("quit"))
            quitSession();
        else if (type == QLatin1String("cancel-job"))
            cancelCurrentJob();
        else
            sendErrorReply("protocol-error", tr("Unknown request type '%1'.").arg(type));
    });
    m_packetReader.start();
}

Session::ProjectDataMode Session::dataModeFromRequest(const QJsonObject &request)
{
    const QString modeString = request.value(QLatin1String("data-mode")).toString();
    if (modeString == QLatin1String("only-if-changed"))
        return ProjectDataMode::OnlyIfChanged;
    if (modeString == QLatin1String("always"))
        return ProjectDataMode::Always;
    return ProjectDataMode::Never;
}

void Session::sendPacket(const QJsonObject &message)
{
    std::cout << SessionPacket::createPacket(message).constData() << std::flush;
}

void Session::setupProject(const QJsonObject &request)
{
    if (m_currentJob) {
        if (qobject_cast<SetupProjectJob *>(m_currentJob)
                && m_currentJob->state() == AbstractJob::StateCanceling) {
            m_resolveRequest = request;
            return;
        }
        sendErrorReply("project-resolved",
                       tr("Cannot start resolving while another job is still running."));
        return;
    }
    m_moduleProperties = modulePropertiesFromRequest(request);
    auto params = SetupProjectParameters::fromJson(request);
    const ProjectDataMode dataMode = dataModeFromRequest(request);
    m_settings = std::make_unique<Settings>(params.settingsDirectory());
    const Preferences prefs(m_settings.get());
    const QString appDir = QDir::cleanPath(QCoreApplication::applicationDirPath());
    params.setSearchPaths(prefs.searchPaths(appDir + QLatin1String(
                                                "/" QBS_RELATIVE_SEARCH_PATH)));
    params.setPluginPaths(prefs.pluginPaths(appDir + QLatin1String(
                                                "/" QBS_RELATIVE_PLUGINS_PATH)));
    params.setLibexecPath(appDir + QLatin1String("/" QBS_RELATIVE_LIBEXEC_PATH));
    params.setOverrideBuildGraphData(true);
    setLogLevelFromRequest(request);
    SetupProjectJob * const setupJob = m_project.setupProject(params, &m_logSink, this);
    m_currentJob = setupJob;
    connectProgressSignals(setupJob);
    connect(setupJob, &AbstractJob::finished, this,
            [this, setupJob, dataMode](bool success) {
        if (!m_resolveRequest.isEmpty()) { // Canceled job was superseded.
            const QJsonObject newRequest = std::move(m_resolveRequest);
            m_resolveRequest = QJsonObject();
            m_currentJob->deleteLater();
            m_currentJob = nullptr;
            setupProject(newRequest);
            return;
        }
        const ProjectData oldProjectData = m_projectData;
        m_project = setupJob->project();
        m_projectData = m_project.projectData();
        QJsonObject reply;
        reply.insert(StringConstants::type(), QLatin1String("project-resolved"));
        if (success)
            insertProjectDataIfNecessary(reply, dataMode, oldProjectData, true);
        else
            insertErrorInfoIfNecessary(reply, setupJob->error());
        sendPacket(reply);
        m_currentJob->deleteLater();
        m_currentJob = nullptr;
    });
}

void Session::buildProject(const QJsonObject &request)
{
    if (!checkNormalRequestPrerequisites("build-done"))
        return;
    const ProductSelection productSelection = getProductSelection(request);
    setLogLevelFromRequest(request);
    auto options = BuildOptions::fromJson(request);
    options.setSettingsDirectory(m_settings->baseDirectory());
    BuildJob * const buildJob = productSelection.products.empty()
            ? m_project.buildAllProducts(options, productSelection.selection, this)
            : m_project.buildSomeProducts(productSelection.products, options, this);
    m_currentJob = buildJob;
    m_moduleProperties = modulePropertiesFromRequest(request);
    const ProjectDataMode dataMode = dataModeFromRequest(request);
    connectProgressSignals(buildJob);
    connect(buildJob, &BuildJob::reportCommandDescription, this,
            [this](const QString &highlight, const QString &message) {
        QJsonObject descData;
        descData.insert(StringConstants::type(), QLatin1String("command-description"));
        descData.insert(QLatin1String("highlight"), highlight);
        descData.insert(StringConstants::messageKey(), message);
        sendPacket(descData);
    });
    connect(buildJob, &BuildJob::reportProcessResult, this, [this](const ProcessResult &result) {
        if (result.success() && result.stdOut().isEmpty() && result.stdErr().isEmpty())
            return;
        QJsonObject resultData = result.toJson();
        resultData.insert(StringConstants::type(), QLatin1String("process-result"));
        sendPacket(resultData);
    });
    connect(buildJob, &BuildJob::finished, this,
            [this, dataMode](bool success) {
        QJsonObject reply;
        reply.insert(StringConstants::type(), QLatin1String("project-built"));
        const ProjectData oldProjectData = m_projectData;
        m_projectData = m_project.projectData();
        if (success)
            insertProjectDataIfNecessary(reply, dataMode, oldProjectData, false);
        else
            insertErrorInfoIfNecessary(reply, m_currentJob->error());
        sendPacket(reply);
        m_currentJob->deleteLater();
        m_currentJob = nullptr;
    });
}

void Session::cleanProject(const QJsonObject &request)
{
    if (!checkNormalRequestPrerequisites("project-cleaned"))
        return;
    setLogLevelFromRequest(request);
    const ProductSelection productSelection = getProductSelection(request);
    const auto options = CleanOptions::fromJson(request);
    m_currentJob = productSelection.products.empty()
            ? m_project.cleanAllProducts(options, this)
            : m_project.cleanSomeProducts(productSelection.products, options, this);
    connectProgressSignals(m_currentJob);
    connect(m_currentJob, &AbstractJob::finished, this, [this](bool success) {
        QJsonObject reply;
        reply.insert(StringConstants::type(), QLatin1String("project-cleaned"));
        if (!success)
            insertErrorInfoIfNecessary(reply, m_currentJob->error());
        sendPacket(reply);
        m_currentJob->deleteLater();
        m_currentJob = nullptr;
    });
}

void Session::installProject(const QJsonObject &request)
{
    if (!checkNormalRequestPrerequisites("install-done"))
        return;
    setLogLevelFromRequest(request);
    const ProductSelection productSelection = getProductSelection(request);
    const auto options = InstallOptions::fromJson(request);
    m_currentJob = productSelection.products.empty()
            ? m_project.installAllProducts(options, productSelection.selection, this)
            : m_project.installSomeProducts(productSelection.products, options, this);
    connectProgressSignals(m_currentJob);
    connect(m_currentJob, &AbstractJob::finished, this, [this](bool success) {
        QJsonObject reply;
        reply.insert(StringConstants::type(), QLatin1String("install-done"));
        if (!success)
            insertErrorInfoIfNecessary(reply, m_currentJob->error());
        sendPacket(reply);
        m_currentJob->deleteLater();
        m_currentJob = nullptr;
    });
}

void Session::addFiles(const QJsonObject &request)
{
    const FileUpdateData data = prepareFileUpdate(request);
    if (data.error.hasError()) {
        sendPacket(data.createErrorReply("files-added", tr("Failed to add files to project: %1")
                                         .arg(data.error.toString())));
        return;
    }
    ErrorInfo error;
    QStringList failedFiles;
#ifdef QBS_ENABLE_PROJECT_FILE_UPDATES
    for (const QString &filePath : data.filePaths) {
        const ErrorInfo e = m_project.addFiles(data.product, data.group, {filePath});
        if (e.hasError()) {
            for (const ErrorItem &ei : e.items())
                error.append(ei);
            failedFiles.push_back(filePath);
        }
    }
#endif
    QJsonObject reply;
    reply.insert(StringConstants::type(), QLatin1String("files-added"));
    insertErrorInfoIfNecessary(reply, error);

    if (failedFiles.size() != data.filePaths.size()) {
        // Note that Project::addFiles() directly changes the existing project data object, so
        // there's no need to retrieve it from m_project.
        insertProjectDataIfNecessary(reply, ProjectDataMode::Always, {}, false);
    }

    if (!failedFiles.isEmpty())
        reply.insert(QLatin1String("failed-files"), QJsonArray::fromStringList(failedFiles));
    sendPacket(reply);
}

void Session::removeFiles(const QJsonObject &request)
{
    const FileUpdateData data = prepareFileUpdate(request);
    if (data.error.hasError()) {
        sendPacket(data.createErrorReply("files-removed",
                                         tr("Failed to remove files from project: %1")
                                         .arg(data.error.toString())));
        return;
    }
    ErrorInfo error;
    QStringList failedFiles;
#ifdef QBS_ENABLE_PROJECT_FILE_UPDATES
    for (const QString &filePath : data.filePaths) {
        const ErrorInfo e = m_project.removeFiles(data.product, data.group, {filePath});
        if (e.hasError()) {
            for (const ErrorItem &ei : e.items())
                error.append(ei);
            failedFiles.push_back(filePath);
        }
    }
#endif
    QJsonObject reply;
    reply.insert(StringConstants::type(), QLatin1String("files-removed"));
    insertErrorInfoIfNecessary(reply, error);
    if (failedFiles.size() != data.filePaths.size())
        insertProjectDataIfNecessary(reply, ProjectDataMode::Always, {}, false);
    if (!failedFiles.isEmpty())
        reply.insert(QLatin1String("failed-files"), QJsonArray::fromStringList(failedFiles));
    sendPacket(reply);
}

void Session::connectProgressSignals(AbstractJob *job)
{
    static QString maxProgressString(QLatin1String("max-progress"));
    connect(job, &AbstractJob::taskStarted, this,
            [this](const QString &description, int maxProgress) {
        QJsonObject msg;
        msg.insert(StringConstants::type(), QLatin1String("task-started"));
        msg.insert(StringConstants::descriptionProperty(), description);
        msg.insert(maxProgressString, maxProgress);
        sendPacket(msg);
    });
    connect(job, &AbstractJob::totalEffortChanged, this, [this](int maxProgress) {
        QJsonObject msg;
        msg.insert(StringConstants::type(), QLatin1String("new-max-progress"));
        msg.insert(maxProgressString, maxProgress);
        sendPacket(msg);
    });
    connect(job, &AbstractJob::taskProgress, this, [this](int progress) {
        QJsonObject msg;
        msg.insert(StringConstants::type(), QLatin1String("task-progress"));
        msg.insert(QLatin1String("progress"), progress);
        sendPacket(msg);
    });
}

static QList<ProductData> getProductsByNameForProject(const ProjectData &project,
                                                      QStringList &productNames)
{
    QList<ProductData> products;
    if (productNames.empty())
        return products;
    for (const ProductData &p : project.products()) {
        for (auto it = productNames.begin(); it != productNames.end(); ++it) {
            if (*it == p.fullDisplayName()) {
                products << p;
                productNames.erase(it);
                if (productNames.empty())
                    return products;
                break;
            }
        }
    }
    for (const ProjectData &p : project.subProjects()) {
        products << getProductsByNameForProject(p, productNames);
        if (productNames.empty())
            break;
    }
    return products;
}

QList<ProductData> Session::getProductsByName(const QStringList &productNames) const
{
    QStringList remainingNames = productNames;
    return getProductsByNameForProject(m_projectData, remainingNames);
}

ProductData Session::getProductByName(const QString &productName) const
{
    const QList<ProductData> products = getProductsByName({productName});
    return products.empty() ? ProductData() : products.first();
}

void Session::getRunEnvironment(const QJsonObject &request)
{
    const char * const replyType = "run-environment";
    if (!checkNormalRequestPrerequisites(replyType))
        return;
    const QString productName = request.value(QLatin1String("product")).toString();
    const ProductData product = getProductByName(productName);
    if (!product.isValid()) {
        sendErrorReply(replyType, tr("No such product '%1'.").arg(productName));
        return;
    }
    const auto inEnv = fromJson<QProcessEnvironment>(
                request.value(QLatin1String("base-environment")));
    const QStringList config = fromJson<QStringList>(request.value(QLatin1String("config")));
    const RunEnvironment runEnv = m_project.getRunEnvironment(product, InstallOptions(), inEnv,
                                                              config, m_settings.get());
    ErrorInfo error;
    const QProcessEnvironment outEnv = runEnv.runEnvironment(&error);
    if (error.hasError()) {
        sendErrorReply(replyType, error);
        return;
    }
    QJsonObject reply;
    reply.insert(StringConstants::type(), QLatin1String(replyType));
    QJsonObject outEnvObj;
    const QStringList keys = outEnv.keys();
    for (const QString &key : keys)
        outEnvObj.insert(key, outEnv.value(key));
    reply.insert(QLatin1String("full-environment"), outEnvObj);
    sendPacket(reply);
}

void Session::getGeneratedFilesForSources(const QJsonObject &request)
{
    const char * const replyType = "generated-files-for-sources";
    if (!checkNormalRequestPrerequisites(replyType))
        return;
    QJsonObject reply;
    reply.insert(StringConstants::type(), QLatin1String(replyType));
    const QJsonArray specs = request.value(StringConstants::productsKey()).toArray();
    QJsonArray resultProducts;
    for (const QJsonValue &p : specs) {
        const QJsonObject productObject = p.toObject();
        const ProductData product = getProductByName(
                    productObject.value(StringConstants::fullDisplayNameKey()).toString());
        if (!product.isValid())
            continue;
        QJsonObject resultProduct;
        resultProduct.insert(StringConstants::fullDisplayNameKey(), product.fullDisplayName());
        QJsonArray results;
        const QJsonArray requests = productObject.value(QLatin1String("requests")).toArray();
        for (const QJsonValue &r : requests) {
            const QJsonObject request = r.toObject();
            const QString filePath = request.value(QLatin1String("source-file")).toString();
            const QStringList tags = fromJson<QStringList>(request.value(QLatin1String("tags")));
            const bool recursive = request.value(QLatin1String("recursive")).toBool();
            const QStringList generatedFiles = m_project.generatedFiles(product, filePath,
                                                                        recursive, tags);
            if (!generatedFiles.isEmpty()) {
                QJsonObject result;
                result.insert(QLatin1String("source-file"), filePath);
                result.insert(QLatin1String("generated-files"),
                              QJsonArray::fromStringList(generatedFiles));
                results << result;
            }
        }
        if (!results.isEmpty()) {
            resultProduct.insert(QLatin1String("results"), results);
            resultProducts << resultProduct;
        }
    }
    reply.insert(StringConstants::productsKey(), resultProducts);
    sendPacket(reply);
}

void Session::releaseProject()
{
    const char * const replyType = "project-released";
    if (!m_project.isValid()) {
        sendErrorReply(replyType, tr("No open project."));
        return;
    }
    if (m_currentJob) {
        m_currentJob->disconnect(this);
        m_currentJob->cancel();
        m_currentJob = nullptr;
    }
    m_project = Project();
    m_projectData = ProjectData();
    m_resolveRequest = QJsonObject();
    QJsonObject reply;
    reply.insert(StringConstants::type(), QLatin1String(replyType));
    sendPacket(reply);
}

void Session::cancelCurrentJob()
{
    if (m_currentJob) {
        if (!m_resolveRequest.isEmpty())
            m_resolveRequest = QJsonObject();
        m_currentJob->cancel();
    }
}

Session::ProductSelection Session::getProductSelection(const QJsonObject &request)
{
    const QJsonValue productSelection = request.value(StringConstants::productsKey());
    if (productSelection.isArray())
        return ProductSelection(getProductsByName(fromJson<QStringList>(productSelection)));
    return ProductSelection(productSelection.toString() == QLatin1String("all")
                            ? Project::ProductSelectionWithNonDefault
                            : Project::ProductSelectionDefaultOnly);
}

Session::FileUpdateData Session::prepareFileUpdate(const QJsonObject &request)
{
    FileUpdateData data;
    const QString productName = request.value(QLatin1String("product")).toString();
    data.product = getProductByName(productName);
    if (data.product.isValid()) {
        const QString groupName = request.value(QLatin1String("group")).toString();
        for (const GroupData &g : data.product.groups()) {
            if (g.name() == groupName) {
                data.group = g;
                break;
            }
        }
        if (!data.group.isValid())
            data.error = tr("Group '%1' not found in product '%2'.").arg(groupName, productName);
    } else {
        data.error = tr("Product '%1' not found in project.").arg(productName);
    }
    const QJsonArray filesArray = request.value(QLatin1String("files")).toArray();
    for (const QJsonValue &v : filesArray)
        data.filePaths << v.toString();
    if (m_currentJob)
        data.error = tr("Cannot update the list of source files while a job is running.");
    if (!m_project.isValid())
        data.error = tr("No valid project. You need to resolve first.");
#ifndef QBS_ENABLE_PROJECT_FILE_UPDATES
    data.error = ErrorInfo(tr("Project file updates are not enabled in this build of qbs."));
#endif
    return data;
}

void Session::insertProjectDataIfNecessary(QJsonObject &reply, ProjectDataMode dataMode,
        const ProjectData &oldProjectData, bool includeTopLevelData)
{
    const bool sendProjectData = dataMode == ProjectDataMode::Always
            || (dataMode == ProjectDataMode::OnlyIfChanged && m_projectData != oldProjectData);
    if (!sendProjectData)
        return;
    QJsonObject projectData = m_projectData.toJson(m_moduleProperties);
    if (includeTopLevelData) {
        QJsonArray buildSystemFiles;
        for (const QString &f : m_project.buildSystemFiles())
            buildSystemFiles.push_back(f);
        projectData.insert(StringConstants::buildDirectoryKey(), m_projectData.buildDirectory());
        projectData.insert(QLatin1String("build-system-files"), buildSystemFiles);
        const Project::BuildGraphInfo bgInfo = m_project.getBuildGraphInfo();
        projectData.insert(QLatin1String("build-graph-file-path"), bgInfo.bgFilePath);
        projectData.insert(QLatin1String("profile-data"),
                           QJsonObject::fromVariantMap(bgInfo.profileData));
        projectData.insert(QLatin1String("overridden-properties"),
                           QJsonObject::fromVariantMap(bgInfo.overriddenProperties));
    }
    reply.insert(QLatin1String("project-data"), projectData);
}

void Session::setLogLevelFromRequest(const QJsonObject &request)
{
    const QString logLevelString = request.value(QLatin1String("log-level")).toString();
    if (logLevelString.isEmpty())
        return;
    for (const LoggerLevel l : {LoggerError, LoggerWarning, LoggerInfo, LoggerDebug, LoggerTrace}) {
        if (logLevelString == logLevelName(l)) {
            m_logSink.setLogLevel(l);
            return;
        }
    }
}

bool Session::checkNormalRequestPrerequisites(const char *replyType)
{
    if (m_currentJob) {
        sendErrorReply(replyType, tr("Another job is still running."));
        return false;
    }
    if (!m_project.isValid()) {
        sendErrorReply(replyType, tr("No valid project. You need to resolve first."));
        return false;
    }
    return true;
}

QStringList Session::modulePropertiesFromRequest(const QJsonObject &request)
{
    return fromJson<QStringList>(request.value(StringConstants::modulePropertiesKey()));
}

void Session::sendErrorReply(const char *replyType, const ErrorInfo &error)
{
    QJsonObject reply;
    reply.insert(StringConstants::type(), QLatin1String(replyType));
    insertErrorInfoIfNecessary(reply, error);
    sendPacket(reply);
}

void Session::sendErrorReply(const char *replyType, const QString &message)
{
    sendErrorReply(replyType, ErrorInfo(message));
}

void Session::insertErrorInfoIfNecessary(QJsonObject &reply, const ErrorInfo &error)
{
    if (error.hasError())
        reply.insert(QLatin1String("error"), error.toJson());
}

void Session::quitSession()
{
    m_logSink.disconnect(this);
    m_packetReader.disconnect(this);
    if (m_currentJob) {
        m_currentJob->disconnect(this);
        connect(m_currentJob, &AbstractJob::finished, qApp, QCoreApplication::quit);
        m_currentJob->cancel();
    } else {
        qApp->quit();
    }
}

QJsonObject Session::FileUpdateData::createErrorReply(const char *type,
                                                      const QString &mainMessage) const
{
    QBS_ASSERT(error.hasError(), return QJsonObject());
    ErrorInfo error(mainMessage);
    for (const ErrorItem &ei : error.items())
        error.append(ei);
    QJsonObject reply;
    reply.insert(StringConstants::type(), QLatin1String(type));
    reply.insert(QLatin1String("error"), error.toJson());
    reply.insert(QLatin1String("failed-files"), QJsonArray::fromStringList(filePaths));
    return reply;
}

} // namespace Internal
} // namespace qbs

#include <session.moc>
