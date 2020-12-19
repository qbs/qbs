/****************************************************************************
**
** Copyright (C) 2018 Ivan Komissarov (abbapoh@gmail.com)
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of Qbs.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#import "Addressbook.pbobjc.h"

#import <Foundation/Foundation.h>

int printUsage(char *argv0)
{
    NSString *programName = [[NSString alloc] initWithUTF8String:argv0];
    NSLog(@"Usage: %@ add|list ADDRESS_BOOK_FILE", programName);
    return -1;
}

NSString *readString(NSString *promt)
{
    NSLog(@"%@", promt);
    NSFileHandle *inputFile = [NSFileHandle fileHandleWithStandardInput];
    NSData *inputData = [inputFile availableData];
    NSString *result = [[NSString alloc] initWithData:inputData encoding:NSUTF8StringEncoding];
    result = [result stringByTrimmingCharactersInSet:
            [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    return result;
}

// This function fills in a Person message based on user input.
void promptForAddress(Person* person)
{
    person.id_p = [readString(@"Enter person ID number:") intValue];
    person.name = readString(@"Enter name:");

    NSString *email = readString(@"Enter email address (blank for none):");
    if ([email length] != 0)
        person.email = email;

    while (true) {
        NSString *number = readString(@"Enter a phone number (or leave blank to finish):");
        if ([number length] == 0)
            break;

        Person_PhoneNumber* phoneNumber = [[Person_PhoneNumber alloc] init];
        phoneNumber.number = number;

        NSString *type = readString(@"Is this a mobile, home, or work phone?:");
        NSLog(@"\"%@\"", type);
        if ([type compare:@"mobile"] == NSOrderedSame)
            phoneNumber.type = Person_PhoneType_Mobile;
        else if ([type compare:@"home"] == NSOrderedSame)
            phoneNumber.type = Person_PhoneType_Home;
        else if ([type compare:@"work"] == NSOrderedSame)
            phoneNumber.type = Person_PhoneType_Work;
        else
            NSLog(@"Unknown phone type. Using default.");

        [person.phonesArray addObject:phoneNumber];
    }
}

// Iterates though all people in the AddressBook and prints info about them.
void listPeople(AddressBook *addressBook)
{
    for (Person *person in addressBook.peopleArray) {
        NSLog(@"Person ID: %d", person.id_p);
        NSLog(@"Person name: %@", person.name);

        if ([person.email length] != 0) {
            NSLog(@"E-mail address: %@", person.email);
        }

        for (Person_PhoneNumber *phoneNumber in person.phonesArray) {
            NSString *phonePrefix;

            switch (phoneNumber.type) {
            case Person_PhoneType_Mobile:
                phonePrefix = @"Mobile phone";
                break;
            case Person_PhoneType_Home:
                phonePrefix = @"Home phone";
                break;
            case Person_PhoneType_Work:
                phonePrefix = @"Work phone";
                break;
            default:
                phonePrefix = @"Unknown phone";
                break;
            }

            NSLog(@"  %@ #: %@", phonePrefix, phoneNumber.number);
        }
        printf("\n");
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3)
        return printUsage(argv[0]);

    @autoreleasepool
    {
        AddressBook *addressBook;
        NSString *filePath = [[NSString alloc] initWithUTF8String:argv[2]];

        // Read the existing address book.
        NSData *data = [NSData dataWithContentsOfFile:filePath];
        if (!data) {
            NSLog(@"%@ : File not found.", filePath);
            addressBook = [[AddressBook alloc] init];
        } else {
            NSError *error;
            addressBook = [AddressBook parseFromData:data error:&error];
            if (!addressBook) {
                NSLog(@"Failed to parse address book.");
                return -1;
            }
        }

        if (strcmp(argv[1], "add") == 0) {
            // Add an address.
            Person *person = [[Person alloc] init];
            promptForAddress(person);
            [addressBook.peopleArray addObject:person];

            if (!data) {
                NSLog(@"Creating a new file.");
            }
            [[addressBook data] writeToFile:filePath atomically:YES];
        } else if (strcmp(argv[1], "list") == 0) {
            listPeople(addressBook);
        } else {
            return printUsage(argv[0]);
        }

        return 0;
    }
}
