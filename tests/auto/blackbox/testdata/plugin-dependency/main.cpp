#if defined(_WIN32) || defined(WIN32)
#   define IMPORT __declspec(dllimport)
#else
#   define IMPORT
#endif

IMPORT void plugin3_hello();
IMPORT void plugin4_hello();
IMPORT void helper1_hello();

int main()
{
    plugin3_hello();
    plugin4_hello();
    helper1_hello();
    return 0;
}
