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

#include <api/projectdata.h>
#include <logging/translator.h>
#include <lsp/basemessage.h>
#include <lsp/completion.h>
#include <lsp/initializemessages.h>
#include <lsp/jsonrpcmessages.h>
#include <lsp/messages.h>
#include <lsp/servercapabilities.h>
#include <lsp/textsynchronization.h>
#include <parser/qmljsastvisitor_p.h>
#include <parser/qmljslexer_p.h>
#include <parser/qmljsparser_p.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/stlutils.h>

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMap>

#include <functional>
#include <optional>
#include <unordered_map>
#ifdef Q_OS_WINDOWS
#include <process.h>
#else
#include <unistd.h>
#endif
#include <utility>

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

class Document {
public:
    bool isPositionUpToDate(const CodePosition &pos) const;
    bool isPositionUpToDate(const lsp::Position &pos) const;

    QString savedContent;
    QString currentContent;
};

static CodePosition posFromLspPos(const lsp::Position &pos)
{
    return {pos.line() + 1, pos.character() + 1};
}

static lsp::Position lspPosFromCodeLocation(const CodeLocation &loc)
{
    return {loc.line() - 1, loc.column() - 1};
}

static QString uriToString(const lsp::DocumentUri &uri)
{
    return uri.toFilePath([](const lsp::Utils::FilePath &fp) { return fp; });
}

static int posToOffset(const CodePosition &pos, const QString &doc);
static int posToOffset(const lsp::Position &pos, const QString &doc)
{
    return posToOffset(posFromLspPos(pos), doc);
}

class AstNodeLocator : public QbsQmlJS::AST::Visitor
{
public:
    AstNodeLocator(int position, QbsQmlJS::AST::UiProgram &ast)
        : m_position(position)
    {
        ast.accept(this);
    }

    QList<QbsQmlJS::AST::Node *> path() const { return m_path; }

private:
    bool preVisit(QbsQmlJS::AST::Node *node) override
    {
        if (int(node->firstSourceLocation().offset) > m_position)
            return false;
        if (int(node->lastSourceLocation().offset) < m_position)
            return false;
        m_path << node;
        return true;
    }

    const int m_position;
    QList<QbsQmlJS::AST::Node *> m_path;
};

static QJsonArray toLspLocations(const QList<CodeLocation> &locations)
{
    struct JsonArray : public QJsonArray
    {
        void reserve(std::size_t) {}
    };
    return transformed<JsonArray>(locations, [](const CodeLocation &loc) {
        const lsp::Position startPos = lspPosFromCodeLocation(loc);
        const lsp::Position endPos(startPos.line(), startPos.character() + 1);
        lsp::Location targetLocation;
        targetLocation.setUri(
            lsp::DocumentUri::fromProtocol(QUrl::fromLocalFile(loc.filePath()).toString()));
        targetLocation.setRange({startPos, endPos});
        return QJsonObject(targetLocation);
    });
}

class LspServer::Private
{
public:
    void setupConnection();
    void handleIncomingData();
    void discardSocket();
    template<typename T> void sendResponse(const T &msg);
    void sendErrorResponse(LspErrorCode code, const QString &message);
    void sendErrorNotification(const QString &message);
    void sendNoSuchDocumentError(const QString &filePath);
    void sendMessage(const lsp::JsonObject &msg);
    void sendMessage(const lsp::JsonRpcMessage &msg);
    void handleCurrentMessage();
    void handleInitializeRequest();
    void handleInitializedNotification();
    void handleGotoDefinitionRequest();
    void handleCompletionRequest();
    void handleShutdownRequest();
    void handleDidOpenNotification();
    void handleDidChangeNotification();
    void handleDidSaveNotification();
    void handleDidCloseNotification();

    bool handleGotoDefViaCodeLinks(
        const QString &sourceFile, const Document *sourceDoc, const CodePosition &sourcePos);
    void handleGotoDefForModuleProperties(
        const QString &sourceFile, const QbsQmlJS::AST::UiQualifiedId *node, int offset);

    QList<ProductData> getRelevantProducts(const QString &sourceFile);

    QLocalServer server;
    QBuffer incomingData;
    lsp::BaseMessage currentMessage;
    QJsonObject messageObject;
    QLocalSocket *socket = nullptr;
    ProjectData projectData;
    CodeLinks codeLinks;
    std::unordered_map<QString, Document> documents;

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

void LspServer::updateProjectData(const ProjectData &projectData, const CodeLinks &codeLinks)
{
    d->projectData = projectData;
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

void LspServer::Private::sendErrorNotification(const QString &message)
{
    lsp::ShowMessageParams params;
    params.setType(lsp::Error);
    params.setMessage(message);
    sendMessage(lsp::ShowMessageNotification(params));
}

void LspServer::Private::sendNoSuchDocumentError(const QString &filePath)
{
    sendErrorNotification(Tr::tr("No such document: '%1'").arg(filePath));
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
    if (method == "textDocument/didOpen")
        return handleDidOpenNotification();
    if (method == "textDocument/didChange")
        return handleDidChangeNotification();
    if (method == "textDocument/didSave")
        return handleDidSaveNotification();
    if (method == "textDocument/didClose")
        return handleDidCloseNotification();
    if (method == "textDocument/definition")
        return handleGotoDefinitionRequest();
    if (method == "textDocument/completion")
        return handleCompletionRequest();

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
    capabilities.setTextDocumentSync({int(lsp::TextDocumentSyncKind::Incremental)});
    lsp::ServerCapabilities::CompletionOptions completionOptions;
    completionOptions.setTriggerCharacters({"."});
    capabilities.setCompletionProvider(completionOptions);
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
    const CodePosition sourcePos = posFromLspPos(params.position());
    const Document *sourceDoc = nullptr;
    if (const auto it = documents.find(sourceFile); it != documents.end())
        sourceDoc = &it->second;

    // First check the codeLinks, where we map locations of Depends items
    // to the location of the corresponding product or module.
    // For a cursor such as this:
    //   Depends { name: "cpp" }
    //                     ^
    // We return the location of the module file that implements the cpp
    // backend for that particular product, e.g. GenericGCC.qbs.
    // In addition, items provided via import paths are looked up here.
    if (handleGotoDefViaCodeLinks(sourceFile, sourceDoc, sourcePos))
        return;

    if (!sourceDoc)
        return sendResponse(nullptr);

    // Now check for
    //   a) elements of the "files" and "references" properties.
    //   b) usages of module properties, e.g. "qbs.toolchain"
    // For a cursor such as this:
    // Group { prefix: "sources/"; files: "main.cpp" }
    //                                      ^
    // We return the absolute path of the file, including the
    // group prefix.
    // Note that if we recorded the location of all source artifacts,
    // we would not need to do any parsing here and also we wouldn't
    // be limited to string literals. But that's probably not worth
    // the additional overhead.
    // For case a), the AST path looks as follows:
    // ... -> UiObjectDefinition -> UiObjectInitializer -> ObjectMemberList
    //     -> UiScriptBinding -> ExpressionStatement [-> ArrayLiteral]
    // If the project file employs the syntactical sugar of using a single
    // string for one-element arrays, then there is no ArrayLiteral; instead,
    // the ExpressionStatement's expression is a StringLiteral that contains
    // the file path.
    // Sometimes, the AST path also ends in an ElementList instead of an ArrayLiteral.
    // This discrepancy is probably a bug in the path() function.
    // Case b) is simpler: We just check whether the last element of the AST path is a
    // qualified id.

    const int offset = posToOffset(params.position(), sourceDoc->currentContent) - 1;
    if (offset < 0 || offset >= sourceDoc->currentContent.length())
        return sendResponse(nullptr);

    using namespace QbsQmlJS;
    using namespace QbsQmlJS::AST;
    Engine engine;
    Lexer lexer(&engine);
    lexer.setCode(sourceDoc->currentContent, 1);
    Parser parser(&engine);
    parser.parse();
    if (!parser.ast())
        return sendResponse(nullptr);

    AstNodeLocator locator(offset, *parser.ast());
    const QList<Node *> &astPath = locator.path();

    const StringLiteral *filePathLiteral = nullptr;
    const ElementList *elementList = nullptr;
    int bindingNodeIndex = 0;
    int objectNodeIndex = 0;
    const Node * const lastNode = astPath.last();
    if (lastNode->kind == Node::Kind_UiQualifiedId) {
        return handleGotoDefForModuleProperties(
            sourceFile, static_cast<const UiQualifiedId *>(lastNode), offset);
    }
    if (lastNode->kind == Node::Kind_ArrayLiteral) {
        bindingNodeIndex = astPath.size() - 3;
        objectNodeIndex = astPath.size() - 6;
        elementList = static_cast<const ArrayLiteral *>(lastNode)->elements;
    } else if (lastNode->kind == Node::Kind_ElementList) {
        bindingNodeIndex = astPath.size() - 4;
        objectNodeIndex = astPath.size() - 7;
        elementList = static_cast<const ElementList *>(lastNode);
    } else if (lastNode->kind == Node::Kind_ExpressionStatement) {
        bindingNodeIndex = astPath.size() - 2;
        objectNodeIndex = astPath.size() - 5;
        const auto filePathExpr = static_cast<const ExpressionStatement *>(lastNode)->expression;
        if (filePathExpr && filePathExpr->kind == Node::Kind_StringLiteral)
            filePathLiteral = static_cast<StringLiteral *>(filePathExpr);
    }
    if (elementList) {
        for (const ElementList *l = elementList; l; l = l->next) {
            if (l->expression->kind != Node::Kind_StringLiteral)
                continue;
            const auto s = static_cast<StringLiteral *>(l->expression);
            if (int(s->firstSourceLocation().offset) > offset)
                break;
            if (int(s->lastSourceLocation().offset + s->lastSourceLocation().length) > offset) {
                filePathLiteral = s;
                break;
            }
        }
    }
    if (!filePathLiteral)
        return sendResponse(nullptr);
    if (objectNodeIndex < 0)
        return sendResponse(nullptr);
    const auto filesNode = astPath.at(bindingNodeIndex);
    if (filesNode->kind != Node::Kind_UiScriptBinding)
        return sendResponse(nullptr);
    const QStringRef bindingName = static_cast<UiScriptBinding *>(filesNode)->qualifiedId->name;
    const bool isFilesProperty = bindingName == "files";
    const bool isReferencesProperty = bindingName == "references";
    if (!isFilesProperty && !isReferencesProperty)
        return sendResponse(nullptr);
    const Node * const objectNode = astPath.at(objectNodeIndex);
    if (objectNode->kind != Node::Kind_UiObjectDefinition)
        return sendResponse(nullptr);
    const SourceLocation loc = objectNode->firstSourceLocation();
    QString absoluteFilePath = filePathLiteral->value.toString();
    const auto sendFilePath = [&] {
        const lsp::Position startPos(0, 0);
        const lsp::Position endPos(0, 1);
        lsp::Location targetLocation;
        targetLocation.setUri(
            lsp::DocumentUri::fromProtocol(QUrl::fromLocalFile(absoluteFilePath).toString()));
        targetLocation.setRange({lsp::Position(0, 0), lsp::Position(0, 1)});
        sendResponse(QJsonObject(targetLocation));
    };
    if (QFileInfo(absoluteFilePath).isAbsolute())
        return sendFilePath();
    if (isFilesProperty) {
        using GroupAndProduct = std::optional<std::pair<GroupData, ProductData>>;
        const std::function<GroupAndProduct(const ProjectData &)> findGroup =
            [&](const ProjectData &project) -> GroupAndProduct {
            for (const ProductData &product : project.products()) {
                for (const GroupData &group : product.groups()) {
                    if (group.location().filePath() == sourceFile
                        && group.location().line() == int(loc.startLine)
                        && group.location().column() == int(loc.startColumn))
                        return std::make_pair(group, product);
                }
            }
            for (const ProjectData &subProject : project.subProjects()) {
                if (const auto &g = findGroup(subProject))
                    return g;
            }
            return {};
        };
        const auto groupAndProduct = findGroup(projectData);
        if (!groupAndProduct)
            return sendResponse(nullptr);
        absoluteFilePath.prepend(groupAndProduct->first.prefix());
        if (QFileInfo(absoluteFilePath).isAbsolute())
            return sendFilePath();
    } else {
        const std::function<std::optional<ProjectData>(const ProjectData &)> findProject =
            [&](const ProjectData &project) -> std::optional<ProjectData> {
            if (project.location().filePath() == sourceFile
                && project.location().line() == int(loc.startLine)
                && project.location().column() == int(loc.startColumn)) {
                return project;
            }
            for (const ProjectData &subProject : project.subProjects()) {
                if (const auto &p = findProject(subProject))
                    return p;
            }
            return {};
        };
        const auto project = findProject(projectData);
        if (!project)
            return sendResponse(nullptr);
    }
    absoluteFilePath = QFileInfo(sourceFile).absoluteDir().filePath(absoluteFilePath);
    sendFilePath();
}

// We operate under the assumption that the client has basic QML support.
// Therefore, we only provide completion for qbs modules and their properties.
// Only a simple prefix match is implemented, with no regard to the contents after the cursor.
void LspServer::Private::handleCompletionRequest()
{
    if (!projectData.isValid())
        return sendResponse(nullptr);

    const lsp::CompletionParams params(messageObject.value(lsp::paramsKey));
    const QString sourceFile = params.textDocument().uri().toLocalFile();
    const Document *sourceDoc = nullptr;
    if (const auto it = documents.find(sourceFile); it != documents.end())
        sourceDoc = &it->second;
    if (!sourceDoc)
        return sendResponse(nullptr);

    const QList<ProductData> relevantProducts = getRelevantProducts(sourceFile);
    if (relevantProducts.isEmpty())
        return sendResponse(nullptr);
    QString identifierPrefix;
    QStringList modulePrefixes;
    const int offset = posToOffset(params.position(), sourceDoc->currentContent) - 1;
    if (offset < 0 || offset >= sourceDoc->currentContent.length())
        return sendResponse(nullptr);
    const auto collectFromRawString = [&] {
        int currentPos = offset;
        const auto constructIdentifier = [&] {
            QString id;
            while (currentPos >= 0) {
                const QChar c = sourceDoc->currentContent.at(currentPos);
                if (!c.isLetterOrNumber() && c != '_')
                    break;
                id.prepend(c);
                --currentPos;
            }
            return id;
        };
        identifierPrefix = constructIdentifier();
        while (true) {
            if (currentPos <= 0 || sourceDoc->currentContent.at(currentPos) != '.')
                return;
            --currentPos;
            const QString modulePrefix = constructIdentifier();
            if (modulePrefix.isEmpty())
                return;
            modulePrefixes.prepend(modulePrefix);
        }
    };

    // Parse the current file. Note that completion usually happens on invalid code, which
    // often confuses the parser so much that it yields unusable results. Therefore, we always
    // gather our input parameters from the raw string. We only use the parse result to skip
    // completion in contexts where it is undesirable.
    QbsQmlJS::Engine engine;
    QbsQmlJS::Lexer lexer(&engine);
    lexer.setCode(sourceDoc->currentContent, 1);
    QbsQmlJS::Parser parser(&engine);
    parser.parse();
    if (parser.ast()) {
        AstNodeLocator locator(offset, *parser.ast());
        const QList<QbsQmlJS::AST::Node *> &astPath = locator.path();
        if (!astPath.isEmpty()) {
            switch (astPath.last()->kind) {
            case QbsQmlJS::AST::Node::Kind_FieldMemberExpression:
            case QbsQmlJS::AST::Node::Kind_UiObjectDefinition:
            case QbsQmlJS::AST::Node::Kind_UiQualifiedId:
            case QbsQmlJS::AST::Node::Kind_UiScriptBinding:
                break;
            default:
                return sendResponse(nullptr);
            }
        }
    }

    collectFromRawString();
    if (modulePrefixes.isEmpty() && identifierPrefix.isEmpty())
        return sendResponse(nullptr); // We do not want to start completion from nothing.

    QJsonArray results;
    QMap<QString, QString> namesAndTypes;
    for (const ProductData &product : relevantProducts) {
        const PropertyMap &moduleProps = product.moduleProperties();
        const QStringList allModules = moduleProps.allModules();
        const QString moduleNameOrPrefix = modulePrefixes.join('.');

        // Case 1: Prefixes match a module name. Identifier can only expand to the name
        // of a module property.
        // Example: "Qt.core.a^" -> "Qt.core.availableBuildVariants"
        if (!modulePrefixes.isEmpty() && allModules.contains(moduleNameOrPrefix)) {
            for (const PropertyMap::PropertyInfo &info :
                 moduleProps.allPropertiesForModule(moduleNameOrPrefix)) {
                if (info.isBuiltin)
                    continue;
                if (!identifierPrefix.isEmpty() && !info.name.startsWith(identifierPrefix))
                    continue;
                namesAndTypes.insert(info.name, info.type);
            }
            continue;
        }

        // Case 2: Isolated identifier. Can only expand to a module name.
        //     Example: "Q^" -> "Qt.core", "Qt.widgets", ...
        // Case 3: Prefixes match a module prefix. Identifier can only expand to a module name.
        //     Example: "Qt.c^" -> "Qt.core", "Qt.concurrent", ...
        QString fullPrefix = identifierPrefix;
        int nameOffset = 0;
        if (!modulePrefixes.isEmpty()) {
            fullPrefix.prepend(moduleNameOrPrefix + '.');
            nameOffset = moduleNameOrPrefix.length() + 1;
        }
        for (const QString &module : allModules) {
            if (module.startsWith(fullPrefix))
                namesAndTypes.insert(module.mid(nameOffset), {});
        }
    }

    for (auto it = namesAndTypes.cbegin(); it != namesAndTypes.cend(); ++it) {
        lsp::CompletionItem item;
        item.setLabel(it.key());
        if (!it.value().isEmpty())
            item.setDetail(it.value());
        results.append(QJsonObject(item));
    };
    sendResponse(results);
}

void LspServer::Private::handleShutdownRequest()
{
    state = State::Shutdown;
    sendResponse(nullptr);
}

void LspServer::Private::handleDidOpenNotification()
{
    const lsp::TextDocumentItem docItem = lsp::DidOpenTextDocumentNotification(messageObject)
                                              .params().value_or(lsp::DidOpenTextDocumentParams())
                                              .textDocument();
    if (!docItem.isValid())
        return sendErrorNotification(Tr::tr("Invalid didOpen parameters."));
    const QString filePath = uriToString(docItem.uri());
    Document &doc = documents[filePath];
    doc.savedContent = doc.currentContent = docItem.text();
}

void LspServer::Private::handleDidChangeNotification()
{
    const auto params = lsp::DidChangeTextDocumentNotification(messageObject)
                            .params().value_or(lsp::DidChangeTextDocumentParams());
    if (!params.isValid())
        return sendErrorNotification(Tr::tr("Invalid didChange parameters."));
    const QString filePath = uriToString(params.textDocument().uri());
    const auto docIt = documents.find(filePath);
    if (docIt == documents.end())
        return sendNoSuchDocumentError(filePath);
    Document &doc = docIt->second;
    const auto changes = params.contentChanges();
    for (const auto &change : changes) {
        const auto range = change.range();
        if (!range) {
            doc.currentContent = change.text();
            continue;
        }
        const int startPos = posToOffset(range->start(), doc.currentContent);
        const int endPos = posToOffset(range->end(), doc.currentContent);
        if (startPos == -1 || endPos == -1 || startPos > endPos)
            return sendErrorResponse(LspErrorResponse::InvalidParams, Tr::tr("Invalid range."));
        doc.currentContent.replace(startPos, endPos - startPos, change.text());
    }
}

void LspServer::Private::handleDidSaveNotification()
{
    const QString filePath = uriToString(lsp::DidSaveTextDocumentNotification(messageObject)
                                             .params().value_or(lsp::DidSaveTextDocumentParams())
                                             .textDocument().uri());
    const auto docIt = documents.find(filePath);
    if (docIt == documents.end())
        return sendNoSuchDocumentError(filePath);
    docIt->second.savedContent = docIt->second.currentContent;

    // TODO: Mark the project data as out of date until the next update(),if the document
    // is in buildSystemFiles().
}

void LspServer::Private::handleDidCloseNotification()
{
    const QString filePath = uriToString(lsp::DidCloseTextDocumentNotification(messageObject)
                                             .params().value_or(lsp::DidCloseTextDocumentParams())
                                             .textDocument().uri());
    const auto docIt = documents.find(filePath);
    if (docIt == documents.end())
        return sendNoSuchDocumentError(filePath);
    documents.erase(docIt);
}

bool LspServer::Private::handleGotoDefViaCodeLinks(
    const QString &sourceFile, const Document *sourceDoc, const CodePosition &sourcePos)
{
    if (sourceDoc && !sourceDoc->isPositionUpToDate(sourcePos))
        return false;
    const auto fileEntry = codeLinks.constFind(sourceFile);
    if (fileEntry == codeLinks.constEnd())
        return false;
    for (auto it = fileEntry->begin(); it != fileEntry->cend(); ++it) {
        if (!it.key().contains(sourcePos))
            continue;
        if (sourceDoc && !sourceDoc->isPositionUpToDate(it.key().end()))
            return false;
        QList<CodeLocation> targets = it.value();
        QBS_ASSERT(!targets.isEmpty(), return false);
        for (auto it = targets.begin(); it != targets.end();) {
            const Document *targetDoc = nullptr;
            if (it->filePath() == sourceFile)
                targetDoc = sourceDoc;
            else if (const auto docIt = documents.find(it->filePath()); docIt != documents.end())
                targetDoc = &docIt->second;
            if (targetDoc && !targetDoc->isPositionUpToDate(CodePosition(it->line(), it->column())))
                it = targets.erase(it);
            else
                ++it;
        }
        const QJsonArray locations = toLspLocations(targets);
        if (locations.size() == 1)
            sendResponse(locations.first().toObject());
        else
            sendResponse(locations);
        return true;
    }
    return false;
}

void LspServer::Private::handleGotoDefForModuleProperties(
    const QString &sourceFile, const QbsQmlJS::AST::UiQualifiedId *node, int offset)
{
    QStringList qualifiedProperty;
    bool cursorIsOnProperty = false;
    for (const QbsQmlJS::AST::UiQualifiedId *qid = node; qid; qid = qid->next) {
        qualifiedProperty << qid->name.toString();
        if (!qid->next && int(qid->firstSourceLocation().offset) <= offset)
            cursorIsOnProperty = true;
    }
    if (qualifiedProperty.size() < 2)
        return sendResponse(nullptr);
    const QList<ProductData> relevantProducts = getRelevantProducts(sourceFile);
    if (relevantProducts.isEmpty())
        return sendResponse(nullptr);
    const QString moduleName = qualifiedProperty.mid(0, qualifiedProperty.size() - 1).join('.');
    const QString propertyName = qualifiedProperty.last();
    QSet<CodeLocation> locations;
    for (const ProductData &p : relevantProducts) {
        for (const ModuleData &m : p.modules()) {
            if (m.name() != moduleName)
                continue;
            if (cursorIsOnProperty) {
                for (const auto &prop : m.properties()) {
                    if (prop.first == propertyName) {
                        locations << prop.second;
                        break;
                    }
                }
            } else {
                locations << m.location();
            }
            break;
        }
    }
    if (locations.isEmpty())
        return sendResponse(nullptr);
    const QJsonArray lspLocations = toLspLocations(toList(locations));
    if (lspLocations.size() == 1)
        return sendResponse(lspLocations.first().toObject());
    sendResponse(lspLocations);
}

// If there are products corresponding to the given source file, return only these.
// Otherwise, return all products.
QList<ProductData> LspServer::Private::getRelevantProducts(const QString &sourceFile)
{
    const QList<ProductData> allProducts = projectData.allProducts();
    QList<ProductData> relevantProducts;
    for (const ProductData &p : allProducts) {
        if (p.location().filePath() == sourceFile)
            relevantProducts << p;
    }
    if (relevantProducts.isEmpty())
        return allProducts;
    return relevantProducts;
}

static int posToOffset(const CodePosition &pos, const QString &doc)
{
    int offset = 0;
    for (int newlines = 0, next = 0; newlines < pos.line() - 1; ++newlines) {
        offset = doc.indexOf('\n', next);
        if (offset == -1)
            return -1;
        next = offset + 1;
    }
    return offset + pos.column();
}

bool Document::isPositionUpToDate(const CodePosition &pos) const
{
    const int origOffset = posToOffset(pos, savedContent);
    if (origOffset > int(currentContent.size()))
        return false;
    return QStringView(currentContent).left(origOffset)
           == QStringView(savedContent).left(origOffset);
}

bool Document::isPositionUpToDate(const lsp::Position &pos) const
{
    return isPositionUpToDate(posFromLspPos(pos));
}

} // namespace qbs::Internal
