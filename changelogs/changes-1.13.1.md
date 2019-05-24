# Important bugfixes
* Qt support: Plugins are no longer linked into static libraries when building against
              a static Qt (QBS-1441).
* Qt support: Fixed excessively long linker command lines (QBS-1441).
* Qt support: Host libraries are now looked up at the right location (QBS-1445).
* Qt support: Fixed failure to find Qt modules in Qt Creator when re-parsing a project that
              hasn't been built yet.
* macOS: Properties in bundle.infoPlist are no longer overridden (QBS-1447).
* iOS: Fixed generation of default Info.plist (QBS-1447).
