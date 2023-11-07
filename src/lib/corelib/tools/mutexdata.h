/****************************************************************************
**
** Copyright (C) 2023 Ivan Komissarov (abbapoh@gmail.com)
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

#pragma once

#include <shared_mutex>

namespace qbs {
namespace Internal {

// adapted version of https://github.com/dragazo/rustex/blob/master/rustex.h

// a data with a mutually exclusive access
template<typename DataType, typename MutexType = std::shared_mutex>
class MutexData
{
public:
    template<typename T, template<typename> typename LockType>
    class ReferenceGuard
    {
        friend class MutexData;
        template<typename U>
        ReferenceGuard(U &&data) noexcept
            : m_ptr(std::addressof(data.m_data))
            , m_lock(data.m_mutex)
        {}
    public:
        ReferenceGuard(const ReferenceGuard &) = delete;
        ReferenceGuard(ReferenceGuard &&) = default;
        ReferenceGuard &operator=(const ReferenceGuard &) = delete;
        ReferenceGuard &operator=(ReferenceGuard &&) = default;

        T &get() const noexcept { return *m_ptr; }
        T &data() const noexcept { return *m_ptr; }
        operator T &() const noexcept { return *m_ptr; }

    private:
        T *m_ptr;
        LockType<MutexType> m_lock;
    };

    using UniqueMutableGuard = ReferenceGuard<DataType, std::unique_lock>;
    using UniqueConstGuard = ReferenceGuard<const DataType, std::unique_lock>;
    using SharedGuard = ReferenceGuard<const DataType, std::shared_lock>;

    template<
        typename ...Args,
        std::enable_if_t<std::is_constructible_v<DataType, Args...>, int> = 0
    >
    explicit MutexData(Args &&...args) : m_data(std::forward<Args>(args)...) {}

    [[nodiscard]] UniqueMutableGuard lock() noexcept { return UniqueMutableGuard{*this}; }
    [[nodiscard]] UniqueConstGuard lock() const noexcept { return UniqueConstGuard{*this}; }

    template<
        typename U = MutexType,
        std::enable_if_t<std::is_same_v<U, std::shared_mutex>, int> = 0
    >
    [[nodiscard]] SharedGuard lock_shared() const noexcept
    { return SharedGuard{*this}; }

private:
    DataType m_data;
    mutable MutexType m_mutex;
};

} // namespace qbs
} // namespace Internal
