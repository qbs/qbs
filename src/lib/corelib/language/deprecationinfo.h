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
#ifndef QBS_DEPRECATIONINFO_H
#define QBS_DEPRECATIONINFO_H

#include <api/languageinfo.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/deprecationwarningmode.h>
#include <tools/error.h>
#include <tools/version.h>

#include <QtCore/qstring.h>

namespace qbs {
namespace Internal {

class DeprecationInfo
{
public:
    explicit DeprecationInfo(const Version &removalVersion,
                             QString additionalUserInfo = QString())
        : m_removalVersion(removalVersion)
        , m_additionalUserInfo(std::move(additionalUserInfo))
    {}
    DeprecationInfo() = default;

    bool isValid() const { return m_removalVersion.isValid(); }
    Version removalVersion() const { return m_removalVersion; }

    ErrorInfo checkForDeprecation(DeprecationWarningMode mode, const QString &name,
                                  const CodeLocation &loc, bool isItem, Logger &logger) const
    {
        if (!isValid())
            return {};
        const Version qbsVersion = LanguageInfo::qbsVersion();
        if (removalVersion() <= qbsVersion) {
            const QString msgTemplate = isItem
                    ? Tr::tr("The item '%1' can no longer be used. It was removed in Qbs %2.")
                    : Tr::tr("The property '%1' can no longer be used. It was removed in Qbs %2.");
            ErrorInfo error(msgTemplate.arg(name, removalVersion().toString()), loc);
            if (!m_additionalUserInfo.isEmpty())
                error.append(m_additionalUserInfo);
            return error;
        }
        const QString msgTemplate = isItem
                ? Tr::tr("The item '%1' is deprecated and will be removed in Qbs %2.")
                : Tr::tr("The property '%1' is deprecated and will be removed in Qbs %2.");
        ErrorInfo error(msgTemplate.arg(name, removalVersion().toString()), loc);
        if (!m_additionalUserInfo.isEmpty())
            error.append(m_additionalUserInfo);
        switch (mode) {
        case DeprecationWarningMode::Error:
            return error;
        case DeprecationWarningMode::On:
            logger.printWarning(error);
            break;
        case DeprecationWarningMode::BeforeRemoval: {
            const Version next(qbsVersion.majorVersion(), qbsVersion.minorVersion() + 1);
            if (removalVersion() == next || removalVersion().minorVersion() == 0)
                logger.printWarning(error);
            break;
        }
        case DeprecationWarningMode::Off:
            break;
        }

        return {};
    }

private:
    Version m_removalVersion;
    QString m_additionalUserInfo;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard.
