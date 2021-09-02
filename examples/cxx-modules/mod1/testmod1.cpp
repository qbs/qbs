//********************************************************
// The following code example is taken from the book
//  C++20 - The Complete Guide
//  by Nicolai M. Josuttis (www.josuttis.com)
//  http://www.cppstd20.com
//
// The code is licensed under a
//  Creative Commons Attribution 4.0 International License
//  http://creativecommons.org/licenses/by/4.0/
//********************************************************

#include <iostream>

import Mod1;

int main()
{
    Customer c1{"Kim"};

    c1.buy("table", 59.90);
    c1.buy(4, "chair", 9.20);

    c1.print();
    std::cout << "  Average: " << c1.averagePrice() << '\n';
}
