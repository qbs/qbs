/****************************************************************************
**
** Copyright (C) 2018 Ivan Komissarov
** Contact: abbapoh@gmail.com
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

#import "Addressbook.pbobjc.h"

#import <Foundation/Foundation.h>

int printUsage(char *argv0)
{
    NSString *programName = [[NSString alloc] initWithUTF8String:argv0];
    NSLog(@"%@", [[NSString alloc] initWithFormat:@"Usage: %@ add|list ADDRESS_BOOK_FILE", programName]);
    [programName release];
    return -1;
}

NSString *readString(NSString *promt)
{
    NSLog(@"%@", promt);
    NSFileHandle *inputFile = [NSFileHandle fileHandleWithStandardInput];
    NSData *inputData = [inputFile availableData];
    NSString *result = [[[NSString alloc]initWithData:inputData encoding:NSUTF8StringEncoding] autorelease];
    result = [[result stringByTrimmingCharactersInSet:
            [NSCharacterSet whitespaceAndNewlineCharacterSet]] autorelease];
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
            NSLog(@"%@", @"Unknown phone type.  Using default.");

        [person.phonesArray addObject:phoneNumber];
    }
}

// Iterates though all people in the AddressBook and prints info about them.
void listPeople(AddressBook *addressBook)
{
    NSArray *people = addressBook.peopleArray;
    for (unsigned i = 0; i < [people count]; i++) {
        Person *person = [people objectAtIndex:i];

        NSLog(@"%@", [[[NSString alloc] initWithFormat:@"Person ID: %d", person.id_p] autorelease]);
        NSLog(@"%@", [[[NSString alloc] initWithFormat:@"Person name: %@", person.name] autorelease]);

        if ([person.email length] != 0) {
            NSLog(@"%@", [[[NSString alloc] initWithFormat:@"E-mail address: %@", person.email] autorelease]);
        }

        NSArray *phones = person.phonesArray;
        for (unsigned j = 0; j < [phones count]; j++) {
            Person_PhoneNumber *phoneNumber = [phones objectAtIndex:j];
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

            NSLog(@"%@", [[[NSString alloc] initWithFormat:@"  %@ #: %@", phonePrefix, phoneNumber.number] autorelease]);
        }
        printf("\n");
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3)
        return printUsage(argv[0]);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    AddressBook *addressBook;// = [AddressBook alloc];
    NSString *filePath = [[[NSString alloc] initWithUTF8String:argv[2]] autorelease];

    // Read the existing address book.
    NSData *data = [NSData dataWithContentsOfFile:filePath];
    if (!data) {
        NSLog(@"%@", [[NSString alloc] initWithFormat:@"%@ : File not found.", filePath]);
        addressBook = [[[AddressBook alloc] init] autorelease];
    } else {
        NSError *error;
        addressBook = [AddressBook parseFromData:data error:&error];
        if (!addressBook) {
            NSLog(@"%@", @"Failed to parse address book.");
            [pool drain];
            return -1;
        }
    }

    if (strcmp(argv[1], "add") == 0) {
        // Add an address.
        Person *person = [[Person alloc] init];
        promptForAddress(person);
        [addressBook.peopleArray addObject:person];

        if (!data) {
            NSLog(@"%@", @"Creating a new file.");
        }
        [[addressBook data] writeToFile:filePath atomically:YES];
    } else if (strcmp(argv[1], "list") == 0) {
        listPeople(addressBook);
    } else {
        [pool drain];
        return printUsage(argv[0]);
    }

    [pool drain];
    return 0;
}
