extern void printHelloC();
extern void printHelloCpp();

#ifdef __APPLE__
extern void printHelloObjc();
extern void printHelloObjcpp();
#endif

int main()
{
    printHelloC();
    printHelloCpp();
#ifdef __APPLE__
    printHelloObjc();
    printHelloObjcpp();
#endif
    return 0;
}
