#include <fstream>

#include <cassert>

int main(int argc, char *argv[])
{
    assert(argc == 3);
    std::ofstream target(argv[2]);
    assert(target);
    target << argv[1];
}
