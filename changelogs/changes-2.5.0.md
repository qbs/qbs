# Language
* `Product` items can now contain `Scanner` items.
* `Group` items can now contain other items, namely `Depends`, `FileTagger`, `Rule`, and `Scanner`.
* `Group` items can now act like `Properties` items. Top-level properties can be set
  via the `product` and `module` prefixes, respectively.
* The conditions of `Properties` items can now overlap, allowing more than one such item
  to contribute to the value of list properties.
* The "else case" semantics for `Properties` items is now deprecated. Instead, `Properties` items
  can be marked as containing fallback values by giving them a condition with the special value
  `undefined`.
* The conditions of `Properties` items now default to `true`.
* Introduced the special `module` variable for `Module` items, which acts like `product`
  in `Product` items.
* Resolving values of path properties in `Export` items relative to the exporting product's location
  is now deprecated.

# C/C++ Support
* Added support for C++20 modules.

# Android support
* Added new properties `Android.sdk.d8Desugaring` and `Android.sdk.extraResourcePackages`.

# Qt Support
* Added new property `Qt.core.translationsPath`.

# Other
* Added experimental WebAssembly support via emscripten.
* The `freedesktop` module now supports localization and deployment of more than one icon.
* The JSON API now supports renaming files.

# Contributors
* Aaron McCarthy
* Christian Kandeler
* Danya Patrushev
* Ivan Komissarov
* Leon Buckel
* Roman Telezhynskyi
