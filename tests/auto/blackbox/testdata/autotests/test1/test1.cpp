#include <cstdlib>
#include <iostream>

int main(int argc, char *[])
{
    if (argc != 2) {
        std::cerr << "This test needs exactly one argument" << std::endl;
        std::cerr << "FAIL" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "PASS" << std::endl;
}
