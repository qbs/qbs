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

#ifndef GENERATORS_XML_PROJECT_WRITER_H
#define GENERATORS_XML_PROJECT_WRITER_H

#include "ixmlnodevisitor.h"

#include <tools/qbs_export.h>

#include <memory>

namespace qbs {
namespace gen {
namespace xml {

class QBS_EXPORT ProjectWriter : public INodeVisitor
{
    Q_DISABLE_COPY(ProjectWriter)
public:
    explicit ProjectWriter(std::ostream *device);
    bool write(const Project *project);

protected:
    QXmlStreamWriter *writer() const;

private:
    void visitPropertyStart(const Property *property) final;
    void visitPropertyEnd(const Property *property) final;

    void visitPropertyGroupStart(const PropertyGroup *propertyGroup) final;
    void visitPropertyGroupEnd(const PropertyGroup *propertyGroup) final;

    std::ostream *m_device = nullptr;
    QByteArray m_buffer;
    std::unique_ptr<QXmlStreamWriter> m_writer;
};

} // namespace xml
} // namespace gen
} // namespace qbs

#endif // GENERATORS_XML_PROJECT_WRITER_H
