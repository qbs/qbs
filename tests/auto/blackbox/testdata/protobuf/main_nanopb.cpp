/****************************************************************************
**
** Copyright (C) 2020 Kai Dohmen (psykai1993@gmail.com)
** Contact: https://www.qt.io/licensing/
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

#include <array>
#include <cassert>
#include <pb_encode.h>

#include "addressbook_nanopb.pb.h"

static_assert(std::is_array<decltype(std::declval<tutorial_Person>().name)>::value, "");
static_assert(std::is_array<decltype(std::declval<tutorial_Person>().email)>::value, "");
static_assert(std::is_array<decltype(std::declval<tutorial_Person>().phones)>::value, "");

bool writeString(pb_ostream_t *stream, const pb_field_t *field, void *const *)
{
    constexpr auto str = "0123456789";
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    return pb_encode_string(stream, reinterpret_cast<const uint8_t*>(str), strlen(str));
}

int main()
{
    std::array<uint8_t, 32> buffer = {};

    tutorial_Person_PhoneNumber phoneNumber;
    phoneNumber.number.funcs.encode = writeString;
    phoneNumber.type = tutorial_Person_PhoneType_WORK;

    auto ostream = pb_ostream_from_buffer(buffer.data(), buffer.size());
    assert(pb_encode(&ostream, tutorial_Person_PhoneNumber_fields, &phoneNumber));

    return 0;
}
