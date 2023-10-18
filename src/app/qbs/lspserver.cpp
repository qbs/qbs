/****************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#include "lspserver.h"

#include <logging/translator.h>
#include <lsp/basemessage.h>
#include <lsp/initializemessages.h>
#include <lsp/jsonrpcmessages.h>
#include <tools/qbsassert.h>
#include <tools/stlutils.h>

#include <QBuffer>
#include <QLocalServer>
#include <QLocalSocket>

#ifdef Q_OS_WINDOWS
#include <process.h>
#else
#include <unistd.h>
#endif

namespace qbs::Internal {

static int getPid()
{
#ifdef Q_OS_WINDOWS
    return _getpid();
#else
    return getpid();
#endif
}

using LspErrorResponse = lsp::ResponseError<std::nullptr_t>;
using LspErrorCode = LspErrorResponse::ErrorCodes;

static CodePosition posFromLspPos(const lsp::Position &pos)
{
    return {pos.line() + 1, pos.character() + 1};
}

static lsp::Position lspPosFromCodeLocation(const CodeLocation &loc)
{
    return {loc.line() - 1, loc.column() - 1};
}

class LspServer::Private
{
public:
    void setupConnection();
    void handleIncomingData();
    void discardSocket();
    template<typename T> void sendResponse(const T &msg);
    void sendErrorResponse(LspErrorCode code, const QString &message);
    void sendMessage(const lsp::JsonObject &msg);
    void sendMessage(const lsp::JsonRpcMessage &msg);
    void handleCurrentMessage();
    void handleInitializeRequest();
    void handleInitializedNotification();
    void handleGotoDefinitionRequest();
    void handleShutdownRequest();

    QLocalServer server;
    QBuffer incomingData;
    lsp::BaseMessage currentMessage;
    QJsonObject messageObject;
    QLocalSocket *socket = nullptr;
    CodeLinks codeLinks;

    enum class State { None, InitRequest, InitNotification, Shutdown };
    State state = State::None;
};

LspServer::LspServer() : d(new Private)
{
    if (!d->server.listen(QString::fromLatin1("qbs-lsp-%1").arg(getPid()))) {
        // This is not fatal, qbs main operation can continue.
        qWarning() << "failed to open lsp socket:" << d->server.errorString();
        return;
    }
    QObject::connect(&d->server, &QLocalServer::newConnection, [this] {
        d->socket = d->server.nextPendingConnection();
        d->setupConnection();
        d->server.close();
    });
}

LspServer::~LspServer() { delete d; }

void LspServer::updateProjectData(const CodeLinks &codeLinks)
{
    d->codeLinks = codeLinks;
}

QString LspServer::socketPath() const { return d->server.fullServerName(); }

void LspServer::Private::setupConnection()
{
    QBS_ASSERT(socket, return);

    QObject::connect(socket, &QLocalSocket::errorOccurred, [this] { discardSocket(); });
    QObject::connect(socket, &QLocalSocket::disconnected, [this] { discardSocket(); });
    QObject::connect(socket, &QLocalSocket::readyRead, [this] { handleIncomingData(); });
    incomingData.open(QIODevice::ReadWrite | QIODevice::Append);
    handleIncomingData();
}

void LspServer::Private::handleIncomingData()
{
    const int pos = incomingData.pos();
    incomingData.write(socket->readAll());
    incomingData.seek(pos);
    QString parseError;
    lsp::BaseMessage::parse(&incomingData, parseError, currentMessage);
    if (!parseError.isEmpty())
        return sendErrorResponse(LspErrorResponse::ParseError, parseError);
    if (currentMessage.isComplete()) {
        incomingData.buffer().remove(0, incomingData.pos());
        incomingData.seek(0);
        handleCurrentMessage();
        currentMessage = {};
        messageObject = {};
        if (socket)
            handleIncomingData();
    }
}

void LspServer::Private::discardSocket()
{
    socket->disconnect();
    socket->deleteLater();
    socket = nullptr;
}

template<typename T> void LspServer::Private::sendResponse(const T &msg)
{
    lsp::Response<T, std::nullptr_t> response(lsp::MessageId(messageObject.value(lsp::idKey)));
    response.setResult(msg);
    sendMessage(response);
}

void LspServer::Private::sendErrorResponse(LspErrorCode code, const QString &message)
{
    lsp::Response<lsp::JsonObject, std::nullptr_t> response(lsp::MessageId(
        messageObject.value(lsp::idKey)));
    lsp::ResponseError<std::nullptr_t> error;
    error.setCode(code);
    error.setMessage(message);
    response.setError(error);
    sendMessage(response);
}

void LspServer::Private::sendMessage(const lsp::JsonObject &msg)
{
    sendMessage(lsp::JsonRpcMessage(msg));
}

void LspServer::Private::sendMessage(const lsp::JsonRpcMessage &msg)
{
    lsp::BaseMessage baseMsg = msg.toBaseMessage();
    socket->write(baseMsg.header());
    socket->write(baseMsg.content);
}

void LspServer::Private::handleCurrentMessage()
{
    messageObject = lsp::JsonRpcMessage(currentMessage).toJsonObject();
    const QString method = messageObject.value(lsp::methodKey).toString();
    if (method == QLatin1String("exit"))
        return discardSocket();
    if (state == State::Shutdown) {
        return sendErrorResponse(LspErrorResponse::InvalidRequest,
                                 Tr::tr("Method '%1' not allowed after shutdown."));
    }
    if (method == "shutdown")
        return handleShutdownRequest();
    if (method == QLatin1String("initialize"))
        return handleInitializeRequest();
    if (state == State::None) {
        return sendErrorResponse(LspErrorResponse::ServerNotInitialized,
                                 Tr::tr("First message must be initialize request."));
    }
    if (method == "initialized")
        return handleInitializedNotification();
    if (method == "textDocument/didOpen" || method == "textDocument/didSave"
            || method == "textDocument/didClose") {
        // TODO: Keep editor state. Refuse to go to definition if there were changes before the
        //       source location. Discard results with changes before the target location.
        //       After a didSave, mark the project data as out of date until the next update().
        //       Only consider files that are in buildSystemFiles().
        return;
    }
    if (method == "textDocument/definition")
        return handleGotoDefinitionRequest();

    sendErrorResponse(LspErrorResponse::MethodNotFound, Tr::tr("This server can do very little."));
}

void LspServer::Private::handleInitializeRequest()
{
    if (state != State::None) {
        return sendErrorResponse(LspErrorResponse::InvalidRequest,
                                 Tr::tr("Received initialize request in initialized state."));
    }
    state = State::InitRequest;

    lsp::ServerInfo serverInfo;
    serverInfo.insert(lsp::nameKey, "qbs");
    serverInfo.insert(lsp::versionKey, QBS_VERSION);
    lsp::InitializeResult result;
    result.insert(lsp::serverInfoKey, serverInfo);
    lsp::ServerCapabilities capabilities; // TODO: hover
    capabilities.setDefinitionProvider(true);
    result.setCapabilities(capabilities);
    sendResponse(result);
}

void LspServer::Private::handleInitializedNotification()
{
    if (state != State::InitRequest) {
        return sendErrorResponse(LspErrorResponse::InvalidRequest,
                                 Tr::tr("Unexpected initialized notification."));
    }
    state = State::InitNotification;
}

void LspServer::Private::handleGotoDefinitionRequest()
{
    const lsp::TextDocumentPositionParams params(messageObject.value(lsp::paramsKey));
    const QString sourceFile = params.textDocument().uri().toLocalFile();
    const auto fileEntry = codeLinks.constFind(sourceFile);
    if (fileEntry == codeLinks.constEnd())
        return sendResponse(nullptr);
    const CodePosition sourcePos = posFromLspPos(params.position());
    for (auto it = fileEntry->cbegin(); it != fileEntry->cend(); ++it) {
        if (!it.key().contains(sourcePos))
            continue;
        const QList<CodeLocation> &targets = it.value();
        QBS_ASSERT(!targets.isEmpty(), return sendResponse(nullptr));
        struct JsonArray : public QJsonArray { void reserve(std::size_t) {}};
        const auto locations = transformed<JsonArray>(targets,
                                                      [](const CodeLocation &loc) {
            const lsp::Position startPos = lspPosFromCodeLocation(loc);
            const lsp::Position endPos(startPos.line(), startPos.character() + 1);
            lsp::Location targetLocation;
            targetLocation.setUri(lsp::DocumentUri::fromProtocol(
                                      QUrl::fromLocalFile(loc.filePath()).toString()));
            targetLocation.setRange({startPos, endPos});
            return QJsonObject(targetLocation);
        });
        if (locations.size() == 1)
            return sendResponse(locations.first().toObject());
        return sendResponse(locations);
    }
    sendResponse(nullptr);
}

void LspServer::Private::handleShutdownRequest()
{
    state = State::Shutdown;
    sendResponse(nullptr);
}

} // namespace qbs::Internal

