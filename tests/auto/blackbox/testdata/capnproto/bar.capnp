@0xc967c84bcca70a1d;

using Foo = import "foo.capnp";

struct Bar {
  foo @0 :Foo.Foo;
  # Use type "Foo" defined in foo.capnp.
}
