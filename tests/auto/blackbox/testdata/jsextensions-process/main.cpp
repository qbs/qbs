#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char *argv[])
{
    if (argc != 2 || std::strcmp(argv[1], "help") != 0) {
        std::fprintf(stderr, "First argument to this program must be 'help'.\n");
        return 1;
    }

    const char *env = std::getenv("SOME_ENV");
    if (!env || std::strcmp(env, "why, hello!") != 0) {
        std::fprintf(stderr, "The SOME_ENV environment variable must be 'why, hello!'.\n");
        return 1;
    }

    std::printf("qbs jsextensions-process test\n");
    return 0;
}
