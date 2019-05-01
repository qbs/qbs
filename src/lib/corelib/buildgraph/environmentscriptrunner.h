/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef QBS_ENVIRONMENTSCRIPTRUNNER_H
#define QBS_ENVIRONMENTSCRIPTRUNNER_H

#include <language/forward_decls.h>

#include <QtCore/qprocess.h>
#include <QtCore/qstringlist.h>

namespace qbs {
namespace Internal {
class RulesEvaluationContext;
class ScriptEngine;

class EnvironmentScriptRunner
{
public:
    EnvironmentScriptRunner(ResolvedProduct *product, RulesEvaluationContext *evalContext,
                            const QProcessEnvironment &env);

    void setupForBuild();
    void setupForRun(const QStringList &config);

private:
    void setupEnvironment();
    ScriptEngine *engine() const;
    const PrivateScriptFunction &getScript(const ResolvedModule *module) const;

    ResolvedProduct * const m_product;
    RulesEvaluationContext * const m_evalContext;
    const QProcessEnvironment m_env;

    QStringList m_runEnvConfig;
    enum EnvType { BuildEnv, RunEnv } m_envType = EnvType::BuildEnv;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard.
