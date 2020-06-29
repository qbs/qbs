@0xc967c84bcca70a1d;

using Foo = import "/imports/foo.capnp";

struct Baz {
  foo @0 :Foo.Foo;
  # Use type "Foo" defined in foo.capnp.
}
