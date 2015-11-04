#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[])
{
    std::cout << "stdout";
    std::cerr << "stderr";
    return atoi(argv[1]);
}
