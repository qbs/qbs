#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char *argv[])
{
    if (argc != 2 || strcmp(argv[1], "help") != 0) {
        fprintf(stderr, "First argument to this program must be 'help'.\n");
        return 1;
    }

    const char *env = std::getenv("SOME_ENV");
    if (!env || strcmp(env, "why, hello!") != 0) {
        fprintf(stderr, "The SOME_ENV environment variable must be 'why, hello!'.\n");
        return 1;
    }

    printf("qbs jsextensions-process test\n");
    return 0;
}
