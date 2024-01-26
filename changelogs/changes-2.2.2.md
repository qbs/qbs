# General
* Fixed name collision check for multi-part modules (QBS-1772).
* Fixed potential assertion when attaching properties on non-present modules (QBS-1776).

# C/C++
* Fixed handling assember flags with MSVC (QBS-1774).

# Qt
* Fixed the qbspkgconfig Qt provider for the case when there is no Qt (QBS-1777).

# Other
* Make protobuf usable without qbspkgconfig again (QBS-1663).
* Add support for the definePrefix option to qbspkgconfig.

# Contributors
* Björn Schäpers
* Christian Kandeler
* Ivan Komissarov
