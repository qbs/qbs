#include <cstdio>

int main(int argc, char **argv)
{
    if (argc < 2)
        return 1;
    FILE *f = fopen(argv[1], "w");
    if (!f)
        return 2;
    fprintf(f, "#include <cstdio>\n\n"
               "int main()\n"
               "{\n    printf(\"Hello World!\\n\");\n    return 0;\n}\n");
    fclose(f);
    return 0;
}
