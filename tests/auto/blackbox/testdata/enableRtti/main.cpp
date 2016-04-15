#include <typeinfo>

class I {
public:
    virtual ~I() { }
    virtual void x() { }
};

class A : public I {
    void x() override { }
};

class B : public I {
    void x() override { }
};

int main() {
    I *a = new A();
    B *b = dynamic_cast<B *>(a);
    (void)b;
    delete a;
    return 0;
}
