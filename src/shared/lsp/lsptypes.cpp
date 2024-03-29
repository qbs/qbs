// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "lsptypes.h"

#include "languageserverprotocoltr.h"
#include "lsputils.h"
#include "textutils.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QMap>
#include <QRegularExpression>
#include <QVector>

namespace lsp {

std::optional<DiagnosticSeverity> Diagnostic::severity() const
{
    if (auto val = optionalValue<int>(severityKey))
        return std::make_optional(static_cast<DiagnosticSeverity>(*val));
    return std::nullopt;
}

std::optional<Diagnostic::Code> Diagnostic::code() const
{
    QJsonValue codeValue = value(codeKey);
    auto it = find(codeKey);
    if (codeValue.isUndefined())
        return std::nullopt;
    QJsonValue::Type type = it.value().type();
    if (type != QJsonValue::String && type != QJsonValue::Double)
        return std::make_optional(Code(QString()));
    return std::make_optional(codeValue.isDouble() ? Code(codeValue.toInt())
                                                   : Code(codeValue.toString()));
}

void Diagnostic::setCode(const Diagnostic::Code &code)
{
    insertVariant<int, QString>(codeKey, code);
}

std::optional<WorkspaceEdit::Changes> WorkspaceEdit::changes() const
{
    auto it = find(changesKey);
    if (it == end())
        return std::nullopt;
    const QJsonObject &changesObject = it.value().toObject();
    Changes changesResult;
    for (const QString &key : changesObject.keys())
        changesResult[DocumentUri::fromProtocol(key)] = LanguageClientArray<TextEdit>(changesObject.value(key)).toList();
    return std::make_optional(changesResult);
}

void WorkspaceEdit::setChanges(const Changes &changes)
{
    QJsonObject changesObject;
    const auto end = changes.end();
    for (auto it = changes.begin(); it != end; ++it) {
        QJsonArray edits;
        for (const TextEdit &edit : it.value())
            edits.append(QJsonValue(edit));
        changesObject.insert(QJsonValue(it.key()).toString(), edits);
    }
    insert(changesKey, changesObject);
}

WorkSpaceFolder::WorkSpaceFolder(const DocumentUri &uri, const QString &name)
{
    setUri(uri);
    setName(name);
}

MarkupOrString::MarkupOrString(const std::variant<QString, MarkupContent> &val)
    : std::variant<QString, MarkupContent>(val)
{ }

MarkupOrString::MarkupOrString(const QString &val)
    : std::variant<QString, MarkupContent>(val)
{ }

MarkupOrString::MarkupOrString(const MarkupContent &val)
    : std::variant<QString, MarkupContent>(val)
{ }

MarkupOrString::MarkupOrString(const QJsonValue &val)
{
    if (val.isString()) {
        emplace<QString>(val.toString());
    } else {
        MarkupContent markupContent(val.toObject());
        if (markupContent.isValid())
            emplace<MarkupContent>(MarkupContent(val.toObject()));
    }
}

QJsonValue MarkupOrString::toJson() const
{
    if (std::holds_alternative<QString>(*this))
        return std::get<QString>(*this);
    if (std::holds_alternative<MarkupContent>(*this))
        return QJsonValue(std::get<MarkupContent>(*this));
    return {};
}

QMap<QString, QString> languageIds()
{
    static const QMap<QString, QString> languages({
            {"Windows Bat","bat"                        },
            {"BibTeX","bibtex"                          },
            {"Clojure","clojure"                        },
            {"Coffeescript","coffeescript"              },
            {"C","c"                                    },
            {"C++","cpp"                                },
            {"C#","csharp"                              },
            {"CSS","css"                                },
            {"Diff","diff"                              },
            {"Dockerfile","dockerfile"                  },
            {"F#","fsharp"                              },
            {"Git commit","git-commit"                  },
            {"Git rebase","git-rebase"                  },
            {"Go","go"                                  },
            {"Groovy","groovy"                          },
            {"Handlebars","handlebars"                  },
            {"HTML","html"                              },
            {"Ini","ini"                                },
            {"Java","java"                              },
            {"JavaScript","javascript"                  },
            {"JSON","json"                              },
            {"LaTeX","latex"                            },
            {"Less","less"                              },
            {"Lua","lua"                                },
            {"Makefile","makefile"                      },
            {"Markdown","markdown"                      },
            {"Objective-C","objective-c"                },
            {"Objective-C++","objective-cpp"            },
            {"Perl6","perl6"                            },
            {"Perl","perl"                              },
            {"PHP","php"                                },
            {"Powershell","powershell"                  },
            {"Pug","jade"                               },
            {"Python","python"                          },
            {"R","r"                                    },
            {"Razor (cshtml)","razor"                   },
            {"Ruby","ruby"                              },
            {"Rust","rust"                              },
            {"Scss (syntax using curly brackets)","scss"},
            {"Sass (indented syntax)","sass"            },
            {"ShaderLab","shaderlab"                    },
            {"Shell Script (Bash)","shellscript"        },
            {"SQL","sql"                                },
            {"Swift","swift"                            },
            {"TypeScript","typescript"                  },
            {"TeX","tex"                                },
            {"Visual Basic","vb"                        },
            {"XML","xml"                                },
            {"XSL","xsl"                                },
            {"YAML","yaml"                              }
    });
    return languages;
}

bool TextDocumentItem::isValid() const
{
    return contains(uriKey) && contains(languageIdKey) && contains(versionKey) && contains(textKey);
}

TextDocumentPositionParams::TextDocumentPositionParams()
    : TextDocumentPositionParams(TextDocumentIdentifier(), Position())
{

}

TextDocumentPositionParams::TextDocumentPositionParams(
        const TextDocumentIdentifier &document, const Position &position)
{
    setTextDocument(document);
    setPosition(position);
}

Position::Position(int line, int character)
{
    setLine(line);
    setCharacter(character);
}

Range::Range(const Position &start, const Position &end)
{
    setStart(start);
    setEnd(end);
}

bool Range::contains(const Range &other) const
{
    if (start() > other.start())
        return false;
    if (end() < other.end())
        return false;
    return true;
}

bool Range::overlaps(const Range &range) const
{
    return !isLeftOf(range) && !range.isLeftOf(*this);
}

QString expressionForGlob(QString globPattern)
{
    const QString anySubDir("qtc_anysubdir_id");
    globPattern.replace("**/", anySubDir);
    QString regexp = QRegularExpression::wildcardToRegularExpression(globPattern);
    regexp.replace(anySubDir,"(.*[/\\\\])*");
    regexp.replace("\\{", "(");
    regexp.replace("\\}", ")");
    regexp.replace(",", "|");
    return regexp;
}

bool DocumentFilter::applies(const Utils::FilePath &fileName) const
{
    if (std::optional<QString> _pattern = pattern()) {
        QRegularExpression::PatternOption option = QRegularExpression::NoPatternOption;
        if (fileName.caseSensitivity() == Qt::CaseInsensitive)
            option = QRegularExpression::CaseInsensitiveOption;
        const QRegularExpression regexp(expressionForGlob(*_pattern), option);
        if (regexp.isValid() && regexp.match(fileName.toString()).hasMatch())
            return true;
    }
    // return false when any of the filter didn't match but return true when no filter was defined
    return !contains(schemeKey) && !contains(languageKey) && !contains(patternKey);
}

Utils::Link Location::toLink(const DocumentUri::PathMapper &mapToHostPath) const
{
    if (!isValid())
        return Utils::Link();

    return Utils::Link(uri().toFilePath(mapToHostPath),
                       range().start().line() + 1,
                       range().start().character());
}

// Ensure %xx like %20 are really decoded using fromPercentEncoding
// Else, a path with spaces would keep its %20 which would cause failure
// to open the file by the text editor. This is the cases with compilers in
// C:\Programs Files on Windows.
DocumentUri::DocumentUri(const QString &other)
    : QUrl(QUrl::fromPercentEncoding(other.toUtf8()))
{ }

DocumentUri::DocumentUri(const Utils::FilePath &other, const PathMapper &mapToServerPath)
    : QUrl(QUrl::fromLocalFile(mapToServerPath(other).path()))
{ }

Utils::FilePath DocumentUri::toFilePath(const PathMapper &mapToHostPath) const
{
    if (isLocalFile()) {
        const Utils::FilePath serverPath = Utils::FilePath::fromUserInput(toLocalFile());
        QBS_ASSERT(mapToHostPath, return serverPath);
        return mapToHostPath(serverPath);
    }
    return {};
}

DocumentUri DocumentUri::fromProtocol(const QString &uri)
{
    return DocumentUri(uri);
}

DocumentUri DocumentUri::fromFilePath(const Utils::FilePath &file, const PathMapper &mapToServerPath)
{
    return DocumentUri(file, mapToServerPath);
}

MarkupKind::MarkupKind(const QJsonValue &value)
{
    m_value = value.toString() == "markdown" ? markdown : plaintext;
}

MarkupKind::operator QJsonValue() const
{
    switch (m_value) {
    case MarkupKind::markdown:
        return "markdown";
    case MarkupKind::plaintext:
        return "plaintext";
    }
    return {};
}

DocumentChange::DocumentChange(const QJsonValue &value)
{
    const QString kind = value["kind"].toString();
    if (kind == "create")
        emplace<CreateFileOperation>(value);
    else if (kind == "rename")
        emplace<RenameFileOperation>(value);
    else if (kind == "delete")
        emplace<DeleteFileOperation>(value);
    else
        emplace<TextDocumentEdit>(value);
}

using DocumentChangeBase = std::variant<TextDocumentEdit, CreateFileOperation, RenameFileOperation, DeleteFileOperation>;

bool DocumentChange::isValid() const
{
    return std::visit([](const auto &v) { return v.isValid(); }, DocumentChangeBase(*this));
}

DocumentChange::operator const QJsonValue () const
{
    return std::visit([](const auto &v) { return QJsonValue(v); }, DocumentChangeBase(*this));
}

CreateFileOperation::CreateFileOperation()
{
    insert(kindKey, "create");
}

QString CreateFileOperation::message(const DocumentUri::PathMapper &mapToHostPath) const
{
    return Tr::tr("Create %1").arg(uri().toFilePath(mapToHostPath).toUserOutput());
}

bool CreateFileOperation::isValid() const
{
    return contains(uriKey) && value(kindKey) == "create";
}

RenameFileOperation::RenameFileOperation()
{
    insert(kindKey, "rename");
}

QString RenameFileOperation::message(const DocumentUri::PathMapper &mapToHostPath) const
{
    return Tr::tr("Rename %1 to %2")
        .arg(oldUri().toFilePath(mapToHostPath).toUserOutput(),
             newUri().toFilePath(mapToHostPath).toUserOutput());
}

bool RenameFileOperation::isValid() const
{
    return contains(oldUriKey) && contains(newUriKey) && value(kindKey) == "rename";
}

DeleteFileOperation::DeleteFileOperation()
{
    insert(kindKey, "delete");
}

QString DeleteFileOperation::message(const DocumentUri::PathMapper &mapToHostPath) const
{
    return Tr::tr("Delete %1").arg(uri().toFilePath(mapToHostPath).toUserOutput());
}

bool DeleteFileOperation::isValid() const
{
    return contains(uriKey) && value(kindKey) == "delete";
}

} // namespace lsp
