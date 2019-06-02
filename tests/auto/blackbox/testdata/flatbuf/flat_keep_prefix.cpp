#include "baz_generated.h"
#include "imported_foo/imported_foo_generated.h"

#include <iostream>

using namespace QbsTest;

int main()
{
    flatbuffers::FlatBufferBuilder builder;

    auto name = builder.CreateString("John Doe");
    auto newFoo = QbsTest::CreateFoo(builder, name, 42);

    auto newBaz = QbsTest::CreateBaz(builder, newFoo);
    builder.Finish(newBaz);

    auto baz = GetBaz(builder.GetBufferPointer());

    assert(baz->foo()->name()->str() == "John Doe");
    assert(baz->foo()->count() == 42);

    std::cout << "The FlatBuffer was successfully created and accessed!" << std::endl;

    return 0;
}
