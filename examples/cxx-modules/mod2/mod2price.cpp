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

module;

#include <vector> // gcc needs this

module Mod2; // implementation unit of module Mod2

import :Order; // import internal partition Order

double Customer::sumPrice() const
{
    double sum = 0.0;
    for (const Order &od : orders) { // ERROR with VC++
                                     //for (const auto& od : orders) {  // OK
        sum += od.count * od.price;
    }
    return sum;
}

double Customer::averagePrice() const
{
    if (orders.empty()) {
        return 0.0;
    }
    return sumPrice() / orders.size();
}
