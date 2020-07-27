#include "bar.capnp.h"

#include <capnp/message.h>

int main()
{
    ::capnp::MallocMessageBuilder message;

    auto bar = message.initRoot<Bar>();
    auto foo = bar.initFoo();
    foo.setStr("hello");

    return 0;
}
