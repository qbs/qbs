#include "foo.capnp.h"

#include <capnp/message.h>

int main()
{
    ::capnp::MallocMessageBuilder message;

    auto foo = message.initRoot<Foo>();
    foo.setStr("hello");

    return 0;
}
