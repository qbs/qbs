/****************************************************************************
**
** Copyright (C) 2021 Ivan Komissarov (abbapoh@gmail.com)
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

#include "tools/qbs_export.h"
#include <tools/stlutils.h>

#include <pkgconfig.h>

#include <QtCore/qobject.h>
#include <QtCore/qvariant.h>

#include <QtScript/qscriptable.h>

#include <memory>

class QProcessEnvironment;

namespace qbs {
namespace Internal {

class QBS_AUTOTEST_EXPORT PkgConfigJs : public QObject, QScriptable
{
    Q_OBJECT
public:

    // can we trick moc here to avoid duplication?
    enum class FlagType {
        LibraryName = toUnderlying(PcPackage::Flag::Type::LibraryName),
        LibraryPath = toUnderlying(PcPackage::Flag::Type::LibraryPath),
        StaticLibraryName = toUnderlying(PcPackage::Flag::Type::StaticLibraryName),
        Framework = toUnderlying(PcPackage::Flag::Type::Framework),
        FrameworkPath = toUnderlying(PcPackage::Flag::Type::FrameworkPath),
        LinkerFlag = toUnderlying(PcPackage::Flag::Type::LinkerFlag),
        IncludePath = toUnderlying(PcPackage::Flag::Type::IncludePath),
        SystemIncludePath = toUnderlying(PcPackage::Flag::Type::SystemIncludePath),
        Define = toUnderlying(PcPackage::Flag::Type::Define),
        CompilerFlag = toUnderlying(PcPackage::Flag::Type::CompilerFlag),
    };
    Q_ENUM(FlagType);

    enum class ComparisonType {
        LessThan,
        GreaterThan,
        LessThanEqual,
        GreaterThanEqual,
        Equal,
        NotEqual,
        AlwaysMatch
    };
    Q_ENUM(ComparisonType);

    static QScriptValue ctor(QScriptContext *context, QScriptEngine *engine);

    explicit PkgConfigJs(
            QScriptContext *context, QScriptEngine *engine, const QVariantMap &options = {});

    Q_INVOKABLE QVariantMap packages() const { return m_packages; }

    // also used in tests
    static PkgConfig::Options convertOptions(const QProcessEnvironment &env, const QVariantMap &map);

private:
    std::unique_ptr<PkgConfig> m_pkgConfig;
    QVariantMap m_packages;
};

} // namespace Internal
} // namespace qbs
