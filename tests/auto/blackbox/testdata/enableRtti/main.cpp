#include <typeinfo>

class A {
    virtual void x() { }
};

class B : public A {
    void x() override { }
};

int main() {
    A a;
    B *b = dynamic_cast<B *>(&a);
    (void)b;
    return 0;
}
