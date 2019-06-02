#include "foo_generated.h"

#include <iostream>

using namespace QbsTest;

int main()
{
    flatbuffers::FlatBufferBuilder builder;
    auto name = builder.CreateString("John Doe");
    auto newFoo = QbsTest::CreateFoo(builder, name, 42);
    builder.Finish(newFoo);

    auto foo = GetFoo(builder.GetBufferPointer());

    assert(foo->name()->str() == "John Doe");
    assert(foo->count() == 42);

    std::cout << "The FlatBuffer was successfully created and accessed!" << std::endl;

    return 0;
}
