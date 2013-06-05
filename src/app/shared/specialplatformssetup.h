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
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTextStream>

QT_FORWARD_DECLARE_CLASS(QProcessEnvironment)

namespace qbs {
class Settings;

class SpecialPlatformsSetup
{
public:
    class Exception
    {
    public:
        Exception(const QString &msg) : errorMessage(msg) {}

        const QString errorMessage;
    };

    class PlatformInfo
    {
    public:
        QString name;
        QStringList targetOS;
        QString toolchainDir;
        QString compilerName;
        QStringList cFlags;
        QStringList cxxFlags;
        QStringList ldFlags;
        QString sysrootDir;
        QString qtBinDir;
        QString qtIncDir;
        QString qtMkspecPath;
        QHash<QString, QString> environment;
    };

    SpecialPlatformsSetup(Settings *settings);
    virtual ~SpecialPlatformsSetup();
    void setup();

    bool helpRequested() const { return m_helpRequested; }
    QString helpString() const;
    static QString tr(const char *str);

protected:
    QString baseDirectory() const { return m_baseDir; }
    QByteArray runProcess(const QString &commandLine, const QProcessEnvironment &env);

private:
    virtual QString defaultBaseDirectory() const = 0;
    virtual QString platformTypeName() const = 0;
    virtual QList<PlatformInfo> gatherPlatformInfo() = 0;

    QString usageString() const;
    void parseCommandLine();
    void setupBaseDir();
    void handleProcessError(const QString &commandLine, const QString &message,
        const QByteArray &output);
    void registerProfile(const PlatformInfo &platformInfo);

    QString m_baseDir;
    QTextStream m_stdout;
    bool m_helpRequested;
    Settings *m_settings;
};

} // namespace qbs
