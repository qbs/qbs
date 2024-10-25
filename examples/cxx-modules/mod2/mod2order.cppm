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


module;              // start module unit with global module fragment

#include <string>

module Mod2:Order;   // internal partition declaration

struct Order {
  int count;
  std::string name;
  double price;

  Order(int c, std::string n, double p)
   : count{c}, name{n}, price{p} {
  }
};

