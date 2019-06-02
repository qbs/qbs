#include "bar_generated.h"

#include <iostream>

using namespace QbsTest;

int main()
{
    flatbuffers::FlatBufferBuilder builder;

    auto name = builder.CreateString("John Doe");
    auto newFoo = QbsTest::CreateFoo(builder, name, 42);

    auto newBar = QbsTest::CreateBar(builder, newFoo);
    builder.Finish(newBar);

    auto bar = GetBar(builder.GetBufferPointer());

    assert(bar->foo()->name()->str() == "John Doe");
    assert(bar->foo()->count() == 42);

    std::cout << "The FlatBuffer was successfully created and accessed!" << std::endl;

    return 0;
}
