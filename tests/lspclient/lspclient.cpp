/****************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <lsp/clientcapabilities.h>
#include <lsp/completion.h>
#include <lsp/initializemessages.h>
#include <lsp/languagefeatures.h>
#include <lsp/textsynchronization.h>

#include <QBuffer>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QLocalSocket>

#include <cstdlib>
#include <iostream>

enum class Command {
    GotoDefinition,
    Completion,
};

class LspClient : public QObject
{
public:
    LspClient(Command command, const QString &socketPath, const QString &codeToInsert,
              int insertLine, int insertColumn,
              const QString &filePath, int line, int column);
    void start();

private:
    void finishWithError(const QString &msg);
    void exit(int code);
    void initiateProtocol();
    void handleIncomingData();
    void sendMessage(const lsp::JsonObject &msg);
    void sendMessage(const lsp::JsonRpcMessage &msg);
    void handleCurrentMessage();
    void handleInitializeReply();
    void openDocument();
    void insertCode();
    void sendRequest();
    void handleResponse();
    void sendGotoDefinitionRequest();
    void handleGotoDefinitionResponse();
    void sendCompletionRequest();
    void handleCompletionResponse();
    lsp::DocumentUri uri() const;
    lsp::DocumentUri::PathMapper mapper() const;

    const Command m_command;
    const QString m_socketPath;
    const QString m_codeToInsert;
    const int m_insertLine;
    const int m_insertColumn;
    const QString m_filePath;
    int m_line;
    int m_column;
    QLocalSocket m_socket;
    QBuffer m_incomingData;
    lsp::BaseMessage m_currentMessage;
    QJsonObject m_messageObject;

    enum class State { Inactive, Connecting, Initializing, RunningCommand }
    m_state = State::Inactive;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QCommandLineOption socketOption({"socket", "s"}, "The server socket to connect to.",
                                          "socket");
    const QCommandLineOption gotoDefinitionOption(
                {"goto-def", "g"}, "Go to definition from the specified location.");
    const QCommandLineOption completionOption(
        {"completion", "c"}, "Request completion at the specified location.");
    const QCommandLineOption insertCodeOption("insert-code",
                                          "A piece of code to insert before doing the actual "
                                          "operation.",
                                          "code");
    const QCommandLineOption insertLocationOption("insert-location",
                                                "The location at which to insert the code.",
                                                "<line>:<column>");
    QCommandLineParser parser;
    parser.addOptions(
        {socketOption,
         insertCodeOption,
         insertLocationOption,
         gotoDefinitionOption,
         completionOption});
    parser.addHelpOption();
    parser.addPositionalArgument("location", "The location at which to operate.",
                                 "<file>:<line>:<column>");
    parser.process(app);

    const auto complainAndExit = [&](const char *text) {
        std::cerr << text << std::endl;
        parser.showHelp(EXIT_FAILURE);
    };

    if (!parser.isSet(socketOption))
        complainAndExit("Socket must be specified.");

    // Initialized to suppress warning. TODO: In C++23, mark lambdas as noreturn instead.
    Command command = Command::GotoDefinition;

    if (parser.isSet(gotoDefinitionOption))
        command = Command::GotoDefinition;
    else if (parser.isSet(completionOption))
        command = Command::Completion;
    else
        complainAndExit("Don't know what to do.");

    if (parser.positionalArguments().size() != 1)
        complainAndExit("Need location.");
    const auto complainAboutLocationString = [&] { complainAndExit("Invalid location."); };
    const QString loc = parser.positionalArguments().first();
    int sep1 = loc.indexOf(':');
    if (sep1 <= 0)
        complainAboutLocationString();
    if (qbs::Internal::HostOsInfo::isWindowsHost()) {
        sep1 = loc.indexOf(':', sep1 + 1);
        if (sep1 < 0)
            complainAboutLocationString();
    }
    const int sep2 = loc.indexOf(':', sep1 + 1);
    if (sep2 == -1 || sep2 == loc.size() - 1)
        complainAboutLocationString();
    const auto extractNumber = [&](const QString &s) {
        bool ok;
        const int n = s.toInt(&ok);
        if (!ok || n <= 0)
            complainAboutLocationString();
        return n;
    };
    const int line = extractNumber(loc.mid(sep1 + 1, sep2 - sep1 - 1));
    const int column = extractNumber(loc.mid(sep2 + 1));

    const QString insertLoc = parser.value(insertLocationOption);
    int insertLine = -1;
    int insertColumn = -1;
    if (insertLoc.isEmpty()) {
        insertLine = line;
        insertColumn = column;
    } else {
        const int sep = insertLoc.indexOf(':');
        if (sep <= 0)
            complainAboutLocationString();
        insertLine = extractNumber(insertLoc.left(sep));
        insertColumn = extractNumber(insertLoc.mid(sep + 1));
    }

    LspClient client(command, parser.value(socketOption), parser.value(insertCodeOption),
                     insertLine, insertColumn,
                     QDir::fromNativeSeparators(loc.left(sep1)), line, column);
    QMetaObject::invokeMethod(&client, &LspClient::start, Qt::QueuedConnection);

    return app.exec();
}

LspClient::LspClient(Command command, const QString &socketPath, const QString &codeToInsert,
                     int insertLine, int insertColumn,
                     const QString &filePath, int line, int column)
    : m_command(command), m_socketPath(socketPath), m_codeToInsert(codeToInsert),
      m_insertLine(insertLine), m_insertColumn(insertColumn),
      m_filePath(filePath), m_line(line), m_column(column)
{
    connect(&m_socket, &QLocalSocket::disconnected, this, [this] {
        finishWithError("Server disconnected unexpectedly.");
    });
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this] {
        finishWithError(QString::fromLatin1("Socket error: %1").arg(m_socket.errorString()));
    });
    connect(&m_socket, &QLocalSocket::connected, this, &LspClient::initiateProtocol);
    connect(&m_socket, &QLocalSocket::readyRead, this, &LspClient::handleIncomingData);
}

void LspClient::start()
{
    m_state = State::Connecting;
    m_incomingData.open(QIODevice::ReadWrite | QIODevice::Append);
    m_socket.connectToServer(m_socketPath);
}

void LspClient::finishWithError(const QString &msg)
{
    std::cerr << qPrintable(msg) << std::endl;
    m_socket.disconnectFromServer();
    exit(EXIT_FAILURE);
}

void LspClient::exit(int code)
{
    m_socket.disconnect(this);
    qApp->exit(code);
}

void LspClient::initiateProtocol()
{
    if (m_state != State::Connecting) {
        finishWithError(QString::fromLatin1("State should be %1, was %2.")
                        .arg(int(State::Connecting), int(m_state)));
        return;
    }
    m_state = State::Initializing;

    lsp::DynamicRegistrationCapabilities definitionCaps;
    definitionCaps.setDynamicRegistration(false);
    lsp::TextDocumentClientCapabilities docCaps;
    docCaps.setDefinition(definitionCaps);
    lsp::ClientCapabilities clientCaps;
    clientCaps.setTextDocument(docCaps);
    lsp::InitializeParams initParams;
    initParams.setCapabilities(clientCaps);
    sendMessage(lsp::InitializeRequest(initParams));
}

void LspClient::handleIncomingData()
{
    const int pos = m_incomingData.pos();
    m_incomingData.write(m_socket.readAll());
    m_incomingData.seek(pos);
    QString parseError;
    lsp::BaseMessage::parse(&m_incomingData, parseError, m_currentMessage);
    if (!parseError.isEmpty()) {
        return finishWithError(QString::fromLatin1("Error parsing server message: %1.")
                               .arg(parseError));
    }
    if (m_currentMessage.isComplete()) {
        m_incomingData.buffer().remove(0, m_incomingData.pos());
        m_incomingData.seek(0);
        handleCurrentMessage();
        m_currentMessage = {};
        m_messageObject = {};
        if (m_socket.state() == QLocalSocket::ConnectedState)
            handleIncomingData();
    }
}

void LspClient::sendMessage(const lsp::JsonObject &msg)
{
    sendMessage(lsp::JsonRpcMessage(msg));
}

void LspClient::sendMessage(const lsp::JsonRpcMessage &msg)
{
    lsp::BaseMessage baseMsg = msg.toBaseMessage();
    m_socket.write(baseMsg.header());
    m_socket.write(baseMsg.content);
}

void LspClient::handleCurrentMessage()
{
    m_messageObject = lsp::JsonRpcMessage(m_currentMessage).toJsonObject();
    switch (m_state) {
    case State::Inactive:
    case State::Connecting:
        finishWithError("Received message in non-connected state.");
        break;
    case State::Initializing:
        handleInitializeReply();
        openDocument();
        insertCode();
        sendRequest();
        break;
    case State::RunningCommand:
        handleResponse();
        break;
    }
}

void LspClient::handleInitializeReply()
{
    lsp::ServerCapabilities serverCaps = lsp::InitializeRequest::Response(
                m_messageObject).result().value_or(lsp::InitializeResult()).capabilities();
    const auto defProvider = serverCaps.definitionProvider();
    if (!defProvider)
        return finishWithError("Expected definition provider.");
    const bool * const defProviderValue = std::get_if<bool>(&(*defProvider));
    if (!defProviderValue || !*defProviderValue)
        return finishWithError("Expected definition provider.");
    sendMessage(lsp::InitializeNotification(lsp::InitializedParams()));
}

void LspClient::openDocument()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return finishWithError(QString::fromLatin1("Could not open '%1': %2")
                                   .arg(m_filePath, file.errorString()));
    }
    lsp::TextDocumentItem item;
    item.setUri(uri());
    item.setVersion(0);
    item.setText(QString::fromUtf8(file.readAll()));
    item.setLanguageId("application/x-qt.qbs+qml");
    sendMessage(lsp::DidOpenTextDocumentNotification(lsp::DidOpenTextDocumentParams(item)));
}

void LspClient::insertCode()
{
    if (m_codeToInsert.isEmpty())
        return;

    lsp::VersionedTextDocumentIdentifier docId;
    docId.setUri(uri());
    docId.setVersion(1);
    lsp::DidChangeTextDocumentParams params(docId);
    lsp::DidChangeTextDocumentParams::TextDocumentContentChangeEvent change;
    const lsp::Position insertPos(m_insertLine - 1, m_insertColumn- 1);
    change.setRange({insertPos, insertPos});
    change.setText(m_codeToInsert);
    params.setContentChanges({change});
    sendMessage(lsp::DidChangeTextDocumentNotification(params));

    if (m_insertLine > m_line || (m_insertLine == m_line && m_insertColumn > m_column))
        return;

    const int newlineCount = m_codeToInsert.count('\n');
    m_line += newlineCount;
    m_column += m_codeToInsert.size() - (newlineCount == 0 ? 0 : m_codeToInsert.lastIndexOf('\n'));
}

void LspClient::sendRequest()
{
    m_state = State::RunningCommand;
    switch (m_command) {
    case Command::GotoDefinition:
        return sendGotoDefinitionRequest();
    case Command::Completion:
        return sendCompletionRequest();
    }
}

void LspClient::handleResponse()
{
    const QString error = m_messageObject.value(lsp::errorKey).toObject()
                              .value("message").toString();
    if (!error.isEmpty())
        return finishWithError(error);

    switch (m_command) {
    case Command::GotoDefinition:
        return handleGotoDefinitionResponse();
    case Command::Completion:
        return handleCompletionResponse();
    }
}

void LspClient::sendGotoDefinitionRequest()
{
    const lsp::TextDocumentIdentifier doc(uri());
    const lsp::Position pos(m_line - 1, m_column - 1);
    sendMessage(lsp::GotoDefinitionRequest({doc, pos}));
}

void LspClient::handleGotoDefinitionResponse()
{
    const lsp::GotoResult result(lsp::GotoDefinitionRequest::Response(m_messageObject)
                                 .result().value_or(lsp::GotoResult()));
    QList<lsp::Utils::Link> links;
    const auto loc2Link = [this](const lsp::Location &loc) { return loc.toLink(mapper()); };
    if (const auto loc = std::get_if<lsp::Location>(&result)) {
        links << loc2Link(*loc);
    } else if (const auto locs = std::get_if<QList<lsp::Location>>(&result)) {
        links = lsp::Utils::transform(*locs, loc2Link);
    }
    for (const lsp::Utils::Link &link : std::as_const(links)) {
        std::cout << qPrintable(link.targetFilePath) << ':' << link.targetLine << ':'
                  << (link.targetColumn + 1) << std::endl;
    }
    exit(EXIT_SUCCESS);
}

void LspClient::sendCompletionRequest()
{
    const lsp::TextDocumentIdentifier doc(uri());
    const lsp::Position pos(m_line - 1, m_column - 1);
    sendMessage(lsp::CompletionRequest({doc, pos}));
}

void LspClient::handleCompletionResponse()
{
    const lsp::CompletionResult result(lsp::CompletionRequest::Response(m_messageObject)
                                           .result()
                                           .value_or(lsp::CompletionResult()));
    if (const auto items = std::get_if<QList<lsp::CompletionItem>>(&result)) {
        for (const lsp::CompletionItem &item : *items) {
            std::cout << qPrintable(item.label());
            if (item.detail())
                std::cout << ' ' << qPrintable(*item.detail());
            std::cout << std::endl;
        }
    }
    exit(EXIT_SUCCESS);
}

lsp::DocumentUri LspClient::uri() const
{
    return lsp::DocumentUri::fromFilePath(lsp::Utils::FilePath::fromUserInput(m_filePath),
                                          mapper());
}

lsp::DocumentUri::PathMapper LspClient::mapper() const
{
    return [](const lsp::Utils::FilePath &fp) { return fp; };
}
