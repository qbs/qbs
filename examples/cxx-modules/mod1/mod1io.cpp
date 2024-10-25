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

module; // start module unit with global module fragment

#include <format>
#include <iostream>
#include <vector>

module Mod1; // implementation unit of module Mod1

void Customer::print() const
{
    // print name:
    std::cout << name << ":\n";
    // print order entries:
    for (const auto &od : orders) {
        std::cout << std::format(
            "{:3} {:14} {:6.2f} {:6.2f}\n", od.count, od.name, od.price, od.count * od.price);
    }
    // print sum:
    std::cout << std::format("{:25} ------\n", ' ');
    std::cout << std::format("{:25} {:6.2f}\n", "    Sum:", sumPrice());
}
