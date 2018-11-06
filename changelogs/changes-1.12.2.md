# Important bugfixes
* The Visual Studio 2017 Build Tools are properly supported now.
* Android NDK r18 is properly supported now.
* Removed invalid assertion that prevented deriving from the Properties item.
* Fixed build error on some BSD hosts (QBS-1395).
* setup-qt fixes:
    * The QtWebkit module is now properly detected (QBS-1399).
    * The case of the qtmain library being called "qt5main" is properly handled now (QBS-767).
    * Building against a Qt that was built with sanitizing support
      works out of the box now (QBS-1387).
