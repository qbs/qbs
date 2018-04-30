#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
    std::cout << "i am the test app" << std::endl;
    const std::string fullHelperExe = std::string(argv[1]) + "/helper-app.exe";
    return std::system(fullHelperExe.c_str()) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
