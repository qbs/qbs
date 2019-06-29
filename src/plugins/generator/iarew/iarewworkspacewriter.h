/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#ifndef QBS_IAREWWORKSPACEWRITER_H
#define QBS_IAREWWORKSPACEWRITER_H

#include "iiarewnodevisitor.h"

namespace qbs {

class IarewWorkspace;

class IarewWorkspaceWriter final : public IIarewNodeVisitor
{
    Q_DISABLE_COPY(IarewWorkspaceWriter)
public:
    explicit IarewWorkspaceWriter(std::ostream *device);
    bool write(const IarewWorkspace *workspace);

private:
    void visitStart(const IarewWorkspace *workspace) final;
    void visitEnd(const IarewWorkspace *workspace) final;

    void visitStart(const IarewProperty *property) final;
    void visitEnd(const IarewProperty *property) final;

    void visitStart(const IarewPropertyGroup *propertyGroup) final;
    void visitEnd(const IarewPropertyGroup *propertyGroup) final;

    std::ostream *m_device = nullptr;
    QByteArray m_buffer;
    std::unique_ptr<QXmlStreamWriter> m_writer;
};

} // namespace qbs

#endif // QBS_IAREWWORKSPACEWRITER_H
