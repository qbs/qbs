# General
* Infinite recursion in property assignments is now properly diagnosed instead of
  triggering a crash (QBS-1793).
* Errors during project resolving print a sort of stack trace now, giving users
  a better idea about what is going wrong.
* The JavaScript backend was switched to `QuickJS-NG`, which is actively maintained.
* Added support for C++ standard library modules - "import std;" and "import std.compat;".
* Fixed support for arm64/armv7 with MSVC.

# Language
* Introduced new property `minimal` to `Depends` item that controls whether the
  dependency should get built in its entirety or only as far as needed by
  the rules in the depending product.
* Relative paths in `Export` items are now resolved relative to the importing product.
* Top-level list property assignments no longer act as fallbacks for `Properties` items, but
  unconditionally contribute to the aggregate value of the property.

# API
* It is now possible to add dependencies to a product.

# Darwin support
* The `bundle` module now uses file tags instead of properties to collect header
  and resource files (QBS-1726).

# Qt Support
* A `Qt.shadertools` module was added.
* moc now uses response files, if necessary.

# Contributors
* Christian Kandeler
* Danya Patrushev
* Ivan Komissarov
* Richard Weickelt
