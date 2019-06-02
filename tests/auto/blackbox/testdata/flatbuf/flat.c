#include "foo_builder.h"

#include <stdio.h>

#undef ns
#define ns(x) FLATBUFFERS_WRAP_NAMESPACE(QbsTest, x) // Specified in the schema.

#define test_assert(x) do { if (!(x)) { assert(0); return -1; }} while (0)

int main()
{
    void *buffer = NULL;
    size_t size = 0;

    flatcc_builder_t builder;
    flatcc_builder_init(&builder);

    flatbuffers_string_ref_t name = flatbuffers_string_create_str(&builder, "John Doe");

    ns(Foo_create_as_root(&builder, name, 42));

    buffer = flatcc_builder_finalize_aligned_buffer(&builder, &size);

    ns(Foo_table_t) foo = ns(Foo_as_root(buffer));

    test_assert(strcmp(ns(Foo_name(foo)), "John Doe") == 0);
    test_assert(ns(Foo_count(foo)) == 42);

    free(buffer);

    flatcc_builder_clear(&builder);

    printf("The FlatBuffer was successfully created and accessed!\n");

    return 0;
}
