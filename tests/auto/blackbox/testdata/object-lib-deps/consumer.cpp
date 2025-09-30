#include <iostream>

void consumed();

void consumer()
{
    consumed();
    std::cout << "consumer" << std::endl;
}
