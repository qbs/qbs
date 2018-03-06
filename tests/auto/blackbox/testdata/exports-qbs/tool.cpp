#include <cstdlib>
#include <fstream>

int main(int argc, char *argv[])
{
    if (argc != 3)
        return EXIT_FAILURE;
    std::ifstream in(argv[1]);
    if (!in)
        return EXIT_FAILURE;
    std::ofstream out(argv[2]);
    if (!out)
        return EXIT_FAILURE;
    char ch;
    while (in.get(ch))
        out.put(ch);
    return in.eof() && out ? EXIT_SUCCESS : EXIT_FAILURE;
}
