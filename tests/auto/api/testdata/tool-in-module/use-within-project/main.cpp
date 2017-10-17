#include <cassert>
#include <fstream>

int main(int argc, char *argv[])
{
    assert(argc == 2);
    std::ofstream file(argv[1]);
    assert(file.is_open());
    file << "content";
}
