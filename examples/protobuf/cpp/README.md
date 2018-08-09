### Addressbook c++ example

This example shows how to build a cpp application that uses Google protobuf.

In order to build this example, you'll need to have a protobuf headers and library installed in the system in locations where QBS can find them.

On Linux, you can install a package to the system.

On macOS, you can use brew or compile and install protobuf manually:
- to /usr/local/
- to any folder, say /Users/<USER>/protobuf. Then you'll need to set protobuf.libraryPath: "/Users/<USER>/protobuf/lib" and protobuf.includePath: "/Users/<USER>/protobuf/include"

On Windows, you have to compile and install protobuf manually to any folder and use libraryPath and includePath as shown above
