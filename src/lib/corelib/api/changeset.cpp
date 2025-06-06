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

#include "changeset.h"

namespace QbsQmlJS {

ChangeSet::ChangeSet()
    : m_string(nullptr), m_error(false)
{
}

ChangeSet::ChangeSet(QList<EditOp> operations)
    : m_string(nullptr), m_operationList(std::move(operations)), m_error(false)
{
}

static bool overlaps(int posA, int lengthA, int posB, int lengthB) {
    if (lengthB > 0) {
        return
                // right edge of B contained in A
                (posA < posB + lengthB && posA + lengthA >= posB + lengthB)
                // left edge of B contained in A
                || (posA <= posB && posA + lengthA > posB)
                // A contained in B
                || (posB < posA && posB + lengthB > posA + lengthA);
    }
    return (posB > posA && posB < posA + lengthA);
}

bool ChangeSet::hasOverlap(int pos, int length)
{
    for (const EditOp &cmd : m_operationList) {
        switch (cmd.type) {
        case EditOp::Replace:
            if (overlaps(pos, length, cmd.pos1, cmd.length1))
                return true;
            break;

        case EditOp::Move:
            if (overlaps(pos, length, cmd.pos1, cmd.length1))
                return true;
            if (cmd.pos2 > pos && cmd.pos2 < pos + length)
                return true;
            break;

        case EditOp::Insert:
            if (cmd.pos1 > pos && cmd.pos1 < pos + length)
                return true;
            break;

        case EditOp::Remove:
            if (overlaps(pos, length, cmd.pos1, cmd.length1))
                return true;
            break;

        case EditOp::Flip:
            if (overlaps(pos, length, cmd.pos1, cmd.length1))
                return true;
            if (overlaps(pos, length, cmd.pos2, cmd.length2))
                return true;
            break;

        case EditOp::Copy:
            if (overlaps(pos, length, cmd.pos1, cmd.length1))
                return true;
            if (cmd.pos2 > pos && cmd.pos2 < pos + length)
                return true;
            break;

        case EditOp::Unset:
            break;
        }
    }

    return false;
}

bool ChangeSet::empty() const
{
    return m_operationList.empty();
}

QList<ChangeSet::EditOp> ChangeSet::operationList() const
{
    return m_operationList;
}

void ChangeSet::clear()
{
    m_string = nullptr;
    m_operationList.clear();
    m_error = false;
}

bool ChangeSet::replace_helper(int pos, int length, const QString &replacement)
{
    if (hasOverlap(pos, length))
        m_error = true;

    EditOp cmd(EditOp::Replace);
    cmd.pos1 = pos;
    cmd.length1 = length;
    cmd.text = replacement;
    m_operationList.push_back(cmd);

    return !m_error;
}

bool ChangeSet::move_helper(int pos, int length, int to)
{
    if (hasOverlap(pos, length)
        || hasOverlap(to, 0)
        || overlaps(pos, length, to, 0))
        m_error = true;

    EditOp cmd(EditOp::Move);
    cmd.pos1 = pos;
    cmd.length1 = length;
    cmd.pos2 = to;
    m_operationList.push_back(cmd);

    return !m_error;
}

bool ChangeSet::insert(int pos, const QString &text)
{
    Q_ASSERT(pos >= 0);

    if (hasOverlap(pos, 0))
        m_error = true;

    EditOp cmd(EditOp::Insert);
    cmd.pos1 = pos;
    cmd.text = text;
    m_operationList.push_back(cmd);

    return !m_error;
}

bool ChangeSet::replace(const Range &range, const QString &replacement)
{ return replace(range.start, range.end, replacement); }

bool ChangeSet::remove(const Range &range)
{ return remove(range.start, range.end); }

bool ChangeSet::move(const Range &range, int to)
{ return move(range.start, range.end, to); }

bool ChangeSet::flip(const Range &range1, const Range &range2)
{ return flip(range1.start, range1.end, range2.start, range2.end); }

bool ChangeSet::copy(const Range &range, int to)
{ return copy(range.start, range.end, to); }

bool ChangeSet::replace(int start, int end, const QString &replacement)
{ return replace_helper(start, end - start, replacement); }

bool ChangeSet::remove(int start, int end)
{ return remove_helper(start, end - start); }

bool ChangeSet::move(int start, int end, int to)
{ return move_helper(start, end - start, to); }

bool ChangeSet::flip(int start1, int end1, int start2, int end2)
{ return flip_helper(start1, end1 - start1, start2, end2 - start2); }

bool ChangeSet::copy(int start, int end, int to)
{ return copy_helper(start, end - start, to); }

bool ChangeSet::remove_helper(int pos, int length)
{
    if (hasOverlap(pos, length))
        m_error = true;

    EditOp cmd(EditOp::Remove);
    cmd.pos1 = pos;
    cmd.length1 = length;
    m_operationList.push_back(cmd);

    return !m_error;
}

bool ChangeSet::flip_helper(int pos1, int length1, int pos2, int length2)
{
    if (hasOverlap(pos1, length1)
        || hasOverlap(pos2, length2)
        || overlaps(pos1, length1, pos2, length2))
        m_error = true;

    EditOp cmd(EditOp::Flip);
    cmd.pos1 = pos1;
    cmd.length1 = length1;
    cmd.pos2 = pos2;
    cmd.length2 = length2;
    m_operationList.push_back(cmd);

    return !m_error;
}

bool ChangeSet::copy_helper(int pos, int length, int to)
{
    if (hasOverlap(pos, length)
        || hasOverlap(to, 0)
        || overlaps(pos, length, to, 0))
        m_error = true;

    EditOp cmd(EditOp::Copy);
    cmd.pos1 = pos;
    cmd.length1 = length;
    cmd.pos2 = to;
    m_operationList.push_back(cmd);

    return !m_error;
}

void ChangeSet::doReplace(const EditOp &replace_helper, QList<EditOp> *replaceList)
{
    Q_ASSERT(replace_helper.type == EditOp::Replace);

    {
        for (EditOp &c : *replaceList) {
            if (replace_helper.pos1 <= c.pos1)
                c.pos1 += replace_helper.text.size();
            if (replace_helper.pos1 < c.pos1)
                c.pos1 -= replace_helper.length1;
        }
    }

    if (m_string) {
        m_string->replace(replace_helper.pos1, replace_helper.length1, replace_helper.text);
    }
}

void ChangeSet::convertToReplace(const EditOp &op, QList<EditOp> *replaceList)
{
    EditOp replace1(EditOp::Replace);
    EditOp replace2(EditOp::Replace);

    switch (op.type) {
    case EditOp::Replace:
        replaceList->push_back(op);
        break;

    case EditOp::Move:
        replace1.pos1 = op.pos1;
        replace1.length1 = op.length1;
        replaceList->push_back(replace1);

        replace2.pos1 = op.pos2;
        replace2.text = textAt(op.pos1, op.length1);
        replaceList->push_back(replace2);
        break;

    case EditOp::Insert:
        replace1.pos1 = op.pos1;
        replace1.text = op.text;
        replaceList->push_back(replace1);
        break;

    case EditOp::Remove:
        replace1.pos1 = op.pos1;
        replace1.length1 = op.length1;
        replaceList->push_back(replace1);
        break;

    case EditOp::Flip:
        replace1.pos1 = op.pos1;
        replace1.length1 = op.length1;
        replace1.text = textAt(op.pos2, op.length2);
        replaceList->push_back(replace1);

        replace2.pos1 = op.pos2;
        replace2.length1 = op.length2;
        replace2.text = textAt(op.pos1, op.length1);
        replaceList->push_back(replace2);
        break;

    case EditOp::Copy:
        replace1.pos1 = op.pos2;
        replace1.text = textAt(op.pos1, op.length1);
        replaceList->push_back(replace1);
        break;

    case EditOp::Unset:
        break;
    }
}

bool ChangeSet::hadErrors()
{
    return m_error;
}

void ChangeSet::apply(QString *s)
{
    m_string = s;
    apply_helper();
    m_string = nullptr;
}

QString ChangeSet::textAt(int pos, int length)
{
    if (m_string) {
        return m_string->mid(pos, length);
    }
    return {};
}

void ChangeSet::apply_helper()
{
    // convert all ops to replace
    QList<EditOp> replaceList;
    {
        while (!m_operationList.empty()) {
            const EditOp cmd(m_operationList.front());
            m_operationList.removeFirst();
            convertToReplace(cmd, &replaceList);
        }
    }

    // execute replaces
    while (!replaceList.empty()) {
        const EditOp cmd(replaceList.front());
        replaceList.removeFirst();
        doReplace(cmd, &replaceList);
    }
}

} // namespace QbsQmlJS

