/****************************************************************************
**
** Copyright (C) 2018 Ivan Komissarov (abbapoh@gmail.com)
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

#include <google/protobuf/util/time_util.h>
#include <string>

#include "addressbook.pb.h"

using google::protobuf::util::TimeUtil;

int main(int /*argc*/, char* /*argv*/[]) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    tutorial::AddressBook addressBook;

    auto person = addressBook.add_people();
    person->set_name("name");
    person->set_id(1);
    person->set_email("email");

    auto phone_number = person->add_phones();
    phone_number->set_number("number");
    phone_number->set_type(tutorial::Person::MOBILE);

    *person->mutable_last_updated() = TimeUtil::SecondsToTimestamp(time(nullptr));

    google::protobuf::ShutdownProtobufLibrary();

    return 0;
}

