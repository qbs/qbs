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

#include <string>
#include <vector>

export module Mod3:Customer; // interface partition declaration

import :Order; // import internal partition to use Order

export class Customer
{
private:
    std::string name;
    std::vector<Order> orders;

public:
    Customer(const std::string &n)
        : name{n}
    {}
    void buy(const std::string &ordername, double price)
    {
        orders.push_back(Order{1, ordername, price});
    }
    void buy(int num, const std::string &ordername, double price)
    {
        orders.push_back(Order{num, ordername, price});
    }
    double sumPrice() const;
    double averagePrice() const;
    void print() const;
};
