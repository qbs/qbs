// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  include <google/protobuf/util/time_util.h>
#  pragma GCC diagnostic pop
#else
#  include <google/protobuf/util/time_util.h>
#endif  // __GNUC__

#include "addressbook.pb.h"

using google::protobuf::util::TimeUtil;

int printUsage(char *argv0)
{
    std::cerr << "Usage:  " << argv0 << "add|list ADDRESS_BOOK_FILE" << std::endl;
    return -1;
}

std::string readString(const std::string &promt)
{
    std::string result;
    std::cout << promt;
    std::getline(std::cin, result);
    return result;
}

// This function fills in a Person message based on user input.
void promptForAddress(tutorial::Person* person)
{
    std::cout << "Enter person ID number: ";
    int id;
    std::cin >> id;
    person->set_id(id);
    std::cin.ignore(256, '\n');

    *person->mutable_name() = readString("Enter name: ");

    const auto email = readString("Enter email address (blank for none): ");
    if (!email.empty())
        person->set_email(email);

    while (true) {
        const auto number = readString("Enter a phone number (or leave blank to finish): ");
        if (number.empty())
            break;

        tutorial::Person::PhoneNumber *phone_number = person->add_phones();
        phone_number->set_number(number);

        const auto type = readString("Is this a mobile, home, or work phone? ");
        if (type == "mobile")
            phone_number->set_type(tutorial::Person::MOBILE);
        else if (type == "home")
            phone_number->set_type(tutorial::Person::HOME);
        else if (type == "work")
            phone_number->set_type(tutorial::Person::WORK);
        else
            std::cout << "Unknown phone type.  Using default." << std::endl;
    }
    *person->mutable_last_updated() = TimeUtil::SecondsToTimestamp(time(NULL));
}

// Iterates though all people in the AddressBook and prints info about them.
void listPeople(const tutorial::AddressBook& address_book)
{
    for (int i = 0; i < address_book.people_size(); i++) {
        const tutorial::Person& person = address_book.people(i);

        std::cout << "Person ID: " << person.id() << std::endl;
        std::cout << "  Name: " << person.name() << std::endl;
        if (!person.email().empty()) {
            std::cout << "  E-mail address: " << person.email() << std::endl;
        }

        for (int j = 0; j < person.phones_size(); j++) {
            const tutorial::Person::PhoneNumber& phone_number = person.phones(j);

            switch (phone_number.type()) {
            case tutorial::Person::MOBILE:
                std::cout << "  Mobile phone #: ";
                break;
            case tutorial::Person::HOME:
                std::cout << "  Home phone #: ";
                break;
            case tutorial::Person::WORK:
                std::cout << "  Work phone #: ";
                break;
            default:
                std::cout << "  Unknown phone #: ";
                break;
            }
            std::cout << phone_number.number() << std::endl;
        }
        if (person.has_last_updated()) {
            std::cout << "  Updated: " << TimeUtil::ToString(person.last_updated()) << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    if (argc != 3)
        return printUsage(argv[0]);

    tutorial::AddressBook address_book;

    // Read the existing address book.
    std::fstream input(argv[2], std::ios::in | std::ios::binary);
    if (!input) {
        std::cout << argv[2] << ": File not found." << std::endl;
    } else if (!address_book.ParseFromIstream(&input)) {
        std::cerr << "Failed to parse address book." << std::endl;
        return -1;
    }

    const std::string mode(argv[1]);
    if (mode == "add") {
        // Add an address.
        promptForAddress(address_book.add_people());

        if (!input)
            std::cout << "Creating a new file." << std::endl;

        // Write the new address book back to disk.
        std::fstream output(argv[2], std::ios::out | std::ios::trunc | std::ios::binary);
        if (!address_book.SerializeToOstream(&output)) {
            std::cerr << "Failed to write address book." << std::endl;
            return -1;
        }
    } else if (mode == "list") {
        listPeople(address_book);
    } else {
        return printUsage(argv[0]);
    }

    // Optional:  Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return 0;
}
