/*
 * Copyright 2019 dvidelabs.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Example on how to build a Monster FlatBuffer.


// Note: while some older C89 compilers are supported when
// -DFLATCC_PORTABLE is defined, this particular sample is known not to
// not work with MSVC 2010 (MSVC 2013 is OK) due to inline variable
// declarations. These are easily move to the start of code blocks, but
// since we follow the step-wise tutorial, it isn't really practical
// in this case. The comment style is technically also in violation of C89.


#include "monster_builder.h"
// <string.h> and <assert.h> already included.

// Convenient namespace macro to manage long namespace prefix.
// The ns macro makes it possible to write `ns(Monster_create(...))`
// instead of `MyGame_Sample_Monster_create(...)`
#undef ns
#define ns(x) FLATBUFFERS_WRAP_NAMESPACE(MyGame_Sample, x) // Specified in the schema.

// A helper to simplify creating vectors from C-arrays.
#define c_vec_len(V) (sizeof(V)/sizeof((V)[0]))

// This allows us to verify result in optimized builds.
#define test_assert(x) do { if (!(x)) { assert(0); return -1; }} while (0)

// Bottom-up approach where we create child objects and store these
// in temporary references before a parent object is created with
// these references.
int create_monster_bottom_up(flatcc_builder_t *B, int flexible)
{
    flatbuffers_string_ref_t weapon_one_name = flatbuffers_string_create_str(B, "Sword");
    uint16_t weapon_one_damage = 3;

    flatbuffers_string_ref_t weapon_two_name = flatbuffers_string_create_str(B, "Axe");
    uint16_t weapon_two_damage = 5;

    // Use the `MyGame_Sample_Weapon_create` shortcut to create Weapons
    // with all the fields set.
    //
    // In the C-API, verbs (here create) always follow the type name
    // (here Weapon), prefixed by the namespace (here MyGame_Sample_):
    // MyGame_Sample_Weapon_create(...), or ns(Weapone_create(...)).
    ns(Weapon_ref_t) sword = ns(Weapon_create(B, weapon_one_name, weapon_one_damage));
    ns(Weapon_ref_t) axe = ns(Weapon_create(B, weapon_two_name, weapon_two_damage));

    // Serialize a name for our monster, called "Orc".
    // The _str suffix indicates the source is an ascii-z string.
    flatbuffers_string_ref_t name = flatbuffers_string_create_str(B, "Orc");

    // Create a `vector` representing the inventory of the Orc. Each number
    // could correspond to an item that can be claimed after he is slain.
    uint8_t treasure[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    flatbuffers_uint8_vec_ref_t inventory;
    // `c_vec_len` is the convenience macro we defined earlier.
    inventory = flatbuffers_uint8_vec_create(B, treasure, c_vec_len(treasure));

    // Here we use a top-down approach locally to build a Weapons vector
    // in-place instead of creating a temporary external vector to use
    // as argument like we did with the `inventory` earlier on, but the
    // overall approach is still bottom-up.
    ns(Weapon_vec_start(B));
    ns(Weapon_vec_push(B, sword));
    ns(Weapon_vec_push(B, axe));
    ns(Weapon_vec_ref_t) weapons = ns(Weapon_vec_end(B));


    // Create a `Vec3`, representing the Orc's position in 3-D space.
    ns(Vec3_t) pos = { 1.0f, 2.0f, 3.0f };


    // Set his hit points to 300 and his mana to 150.
    uint16_t hp = 300;
    // The default value is 150, so we will never store this field.
    uint16_t mana = 150;

    // Create the equipment union. In the C++ language API this is given
    // as two arguments to the create call, or as two separate add
    // operations for the type and the table reference. Here we create
    // a single union value that carries both the type and reference.
    ns(Equipment_union_ref_t) equipped = ns(Equipment_as_Weapon(axe));

    if (!flexible) {
        // Finally, create the monster using the `Monster_create` helper function
        // to set all fields.
        //
        // Note that the Equipment union only take up one argument in C, where
        // C++ takes a type and an object argument.
        ns(Monster_create_as_root(B, &pos, mana, hp, name, inventory, ns(Color_Red),
                             weapons, equipped));

        // Unlike C++ we do not use a Finish call. Instead we use the
        // `create_as_root` action which has better type safety and
        // simplicity.
        //
        // However, we can also express this as:
        //
        // ns(Monster_ref_t) orc = ns(Monster_create(B, ...));
        // flatcc_builder_buffer_create(orc);
        //
        // In this approach the function should return the orc and
        // let a calling function handle the flatcc_buffer_create call
        // for a more composable setup that is also able to create child
        // monsters. In general, `flatcc_builder` calls are best isolated
        // in a containing driver function.

    } else {

        // A more flexible approach where we mix bottom-up and top-down
        // style. We still create child objects first, but then create
        // a top-down style monster object that we can manipulate in more
        // detail.

        // It is important to pair `start_as_root` with `end_as_root`.
        ns(Monster_start_as_root(B));
        ns(Monster_pos_create(B, 1.0f, 2.0f, 3.0f));
        // or alternatively
        //ns(Monster_pos_add(&pos);

        ns(Monster_hp_add(B, hp));
        // Notice that `Monser_name_add` adds a string reference unlike the
        // add_str and add_strn variants.
        ns(Monster_name_add(B, name));
        ns(Monster_inventory_add(B, inventory));
        ns(Monster_color_add(B, ns(Color_Red)));
        ns(Monster_weapons_add(B, weapons));
        ns(Monster_equipped_add(B, equipped));
        // Complete the monster object and make it the buffer root object.
        ns(Monster_end_as_root(B));

        // We could also drop the `as_root` suffix from Monster_start/end(B)
        // and add the table as buffer root later:
        //
        // ns(Monster_ref_t) orc = ns(Monster_start(B));
        // ...
        // ns(Monster_ref_t) orc = ns(Monster_end(B));
        // flatcc_builder_buffer_create(orc);
        //
        // It is best to keep the `flatcc_builder` calls in a containing
        // driver function for modularity.
    }
    return 0;
}

// Alternative top-down approach where parent objects are created before
// their children. We only need to save one reference because the `axe`
// object is used in two places effectively making the buffer object
// graph a DAG.
int create_monster_top_down(flatcc_builder_t *B)
{
    uint8_t treasure[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    size_t treasure_count = c_vec_len(treasure);
    ns(Weapon_ref_t) axe;

    // NOTE: if we use end_as_root, we MUST also start as root.
    ns(Monster_start_as_root(B));
    ns(Monster_pos_create(B, 1.0f, 2.0f, 3.0f));
    ns(Monster_hp_add(B, 300));
    //ns(Monster_mana_add(B, 150));
    // We use create_str instead of add because we have no existing string reference.
    ns(Monster_name_create_str(B, "Orc"));
    // Again we use create because we no existing vector object, only a C-array.
    ns(Monster_inventory_create(B, treasure, treasure_count));
    ns(Monster_color_add(B, ns(Color_Red)));
    if (1) {
        ns(Monster_weapons_start(B));
        ns(Monster_weapons_push_create(B, flatbuffers_string_create_str(B, "Sword"), 3));
        // We reuse the axe object later. Note that we dereference a pointer
        // because push always returns a short-term pointer to the stored element.
        // We could also have created the axe object first and simply pushed it.
        axe = *ns(Monster_weapons_push_create(B, flatbuffers_string_create_str(B, "Axe"), 5));
        ns(Monster_weapons_end(B));
    } else {
        // We can have more control with the table elements added to a vector:
        //
        ns(Monster_weapons_start(B));
        ns(Monster_weapons_push_start(B));
        ns(Weapon_name_create_str(B, "Sword"));
        ns(Weapon_damage_add(B, 3));
        ns(Monster_weapons_push_end(B));
        ns(Monster_weapons_push_start(B));
        ns(Weapon_name_create_str(B, "Axe"));
        ns(Weapon_damage_add(B, 5));
        axe = *ns(Monster_weapons_push_end(B));
        ns(Monster_weapons_end(B));
    }
    // Unions can get their type by using a type-specific add/create/start method.
    ns(Monster_equipped_Weapon_add(B, axe));

    ns(Monster_end_as_root(B));
    return 0;
}

// This isn't strictly needed because the builder already included the reader,
// but we would need it if our reader were in a separate file.
#include "monster_reader.h"

#undef ns
#define ns(x) FLATBUFFERS_WRAP_NAMESPACE(MyGame_Sample, x) // Specified in the schema.

int access_monster_buffer(const void *buffer)
{
    // Note that we use the `table_t` suffix when reading a table object
    // as opposed to the `ref_t` suffix used during the construction of
    // the buffer.
    ns(Monster_table_t) monster = ns(Monster_as_root(buffer));

    // Note: root object pointers are NOT the same as the `buffer` pointer.

    // Make sure the buffer is accessible.
    test_assert(monster != 0);

    uint16_t hp = ns(Monster_hp(monster));
    uint16_t mana = ns(Monster_mana(monster));
    // This is just a const char *, but it also supports a fast length operation.
    flatbuffers_string_t name = ns(Monster_name(monster));
    size_t name_len = flatbuffers_string_len(name);

    test_assert(hp == 300);
    // Since 150 is the default, we are reading a value that wasn't stored.
    test_assert(mana == 150);
    test_assert(0 == strcmp(name, "Orc"));
    test_assert(name_len == strlen("Orc"));

    int hp_present = ns(Monster_hp_is_present(monster)); // 1
    int mana_present = ns(Monster_mana_is_present(monster)); // 0
    test_assert(hp_present);
    test_assert(!mana_present);

    ns(Vec3_struct_t) pos = ns(Monster_pos(monster));
    // Make sure pos has been set.
    test_assert(pos != 0);
    float x = ns(Vec3_x(pos));
    float y = ns(Vec3_y(pos));
    float z = ns(Vec3_z(pos));

    // The literal `f` suffix is important because double literals does
    // not always map cleanly to 32-bit represention even with only a few digits:
    // `1.0 == 1.0f`, but `3.2 != 3.2f`.
    test_assert(x == 1.0f);
    test_assert(y == 2.0f);
    test_assert(z == 3.0f);

    // We can also read the position into a C-struct. We have to copy
    // because we generally do not know if the native endian format
    // matches the one stored in the buffer (pe: protocol endian).
    ns(Vec3_t) pos_vec;
    // `pe` indicates endian conversion from protocol to native.
    ns(Vec3_copy_from_pe(&pos_vec, pos));
    test_assert(pos_vec.x == 1.0f);
    test_assert(pos_vec.y == 2.0f);
    test_assert(pos_vec.z == 3.0f);

    // This is a const uint8_t *, but it shouldn't be accessed directly
    // to ensure proper endian conversion. However, uint8 (ubyte) are
    // not sensitive endianness, so we *could* have accessed it directly.
    // The compiler likely optimizes this so that it doesn't matter.
    flatbuffers_uint8_vec_t inv = ns(Monster_inventory(monster));
    size_t inv_len = flatbuffers_uint8_vec_len(inv);
    // Make sure the inventory has been set.
    test_assert(inv != 0);
    // If `inv` were absent, the length would 0, so the above test is redundant.
    test_assert(inv_len == 10);
    // Index 0 is the first, index 2 is the third.
    // NOTE: C++ uses the `Get` terminology for vector elemetns, C use `at`.
    uint8_t third_item = flatbuffers_uint8_vec_at(inv, 2);
    test_assert(third_item == 2);

    ns(Weapon_vec_t) weapons = ns(Monster_weapons(monster));
    size_t weapons_len = ns(Weapon_vec_len(weapons));
    test_assert(weapons_len == 2);
    // We can use `const char *` instead of `flatbuffers_string_t`.
    const char *second_weapon_name = ns(Weapon_name(ns(Weapon_vec_at(weapons, 1))));
    uint16_t second_weapon_damage =  ns(Weapon_damage(ns(Weapon_vec_at(weapons, 1))));
    test_assert(second_weapon_name != 0 && strcmp(second_weapon_name, "Axe") == 0);
    test_assert(second_weapon_damage == 5);

    // Access union type field.
    if (ns(Monster_equipped_type(monster)) == ns(Equipment_Weapon)) {
        // Cast to appropriate type:
        // C does not require the cast to Weapon_table_t, but C++ does.
        ns(Weapon_table_t) weapon = (ns(Weapon_table_t)) ns(Monster_equipped(monster));
        const char *weapon_name = ns(Weapon_name(weapon));
        uint16_t weapon_damage = ns(Weapon_damage(weapon));

        test_assert(0 == strcmp(weapon_name, "Axe"));
        test_assert(weapon_damage == 5);
    }
    return 0;
}

#include <stdio.h>

int main(int argc, char *argv[])
{
    // Create a `FlatBufferBuilder`, which will be used to create our
    // monsters' FlatBuffers.
    flatcc_builder_t builder;
    void  *buf;
    size_t size;

    // Silence warnings.
    (void)argc;
    (void)argv;

    // Initialize the builder object.
    flatcc_builder_init(&builder);
    test_assert(0 == create_monster_bottom_up(&builder, 0));

    // Allocate and extract a readable buffer from internal builder heap.
    // The returned buffer must be deallocated using `free`.
    // NOTE: Finalizing the buffer does NOT change the builder, it
    // just creates a snapshot of the builder content.
    // NOTE2: finalize_buffer uses malloc while finalize_aligned_buffer
    // uses a portable aligned allocation method. Often the malloc
    // version is sufficient, but won't work for all schema on all
    // systems. If the buffer is written to disk or network, but not
    // accessed in memory, `finalize_buffer` is also sufficient.
    buf = flatcc_builder_finalize_aligned_buffer(&builder, &size);
    //buf = flatcc_builder_finalize_buffer(&builder, &size);

    // We now have a FlatBuffer we can store on disk or send over a network.
    // ** file/network code goes here :) **
    // Instead, we're going to access it right away (as if we just received it).
    //access_monster_buffer(buf);

    // prior to v0.5.0, use `aligned_free`
    flatcc_builder_aligned_free(buf);
    //free(buf);
    //
    // The builder object can optionally be reused after a reset which
    // is faster than creating a new builder. Subsequent use might
    // entirely avoid temporary allocations until finalizing the buffer.
    flatcc_builder_reset(&builder);
    test_assert(0 == create_monster_bottom_up(&builder, 1));
    buf = flatcc_builder_finalize_aligned_buffer(&builder, &size);
    access_monster_buffer(buf);
    flatcc_builder_aligned_free(buf);
    flatcc_builder_reset(&builder);
    create_monster_top_down(&builder);
    buf = flatcc_builder_finalize_buffer(&builder, &size);
    test_assert(0 == access_monster_buffer(buf));
    free(buf);
    // Eventually the builder must be cleaned up:
    flatcc_builder_clear(&builder);

    printf("The FlatBuffer was successfully created and accessed!\n");

    return 0;
}
