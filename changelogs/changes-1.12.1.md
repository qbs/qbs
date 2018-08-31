# Important bugfixes
* Lifted the restriction that the -march option cannot appear in cpp.*Flags (QBS-1018).
* All required header files get installed now (QBS-1370).
* Fixed rpaths not ending up on the command line under certain circumstances (QBS-1372).
* Fixed possible crash when scanning qrc files (QBS-1375).
* Fixed spurious re-building of .pc and .qbs module files.
* Fixed possible crash on storing a build graph after re-resolving.
* Fixed possible assertion on input artifacts with alwaysUpdated == false.
