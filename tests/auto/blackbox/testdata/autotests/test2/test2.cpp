#include <cstdlib>
#include <fstream>
#include <iostream>

int main()
{
    std::ifstream input("test2-resource.txt");
    if (!input.is_open()) {
        std::cerr << "Test resource not found";
        return EXIT_FAILURE;
    }
    std::cout << "PASS" << std::endl;
}
