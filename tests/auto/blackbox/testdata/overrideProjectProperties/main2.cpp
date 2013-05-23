#if defined(_WIN32)
#   define IMPORT __declspec(dllimport)
#else
#   define IMPORT
#endif

void IMPORT helperFunc();

int main()
{
    helperFunc();
    return 0;
}

