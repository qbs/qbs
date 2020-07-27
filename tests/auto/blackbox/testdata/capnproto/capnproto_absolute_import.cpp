#include "baz.capnp.h"

#include <capnp/message.h>

int main()
{
    ::capnp::MallocMessageBuilder message;

    auto baz = message.initRoot<Baz>();
    auto foo = baz.initFoo();
    foo.setStr("hello");

    return 0;
}
