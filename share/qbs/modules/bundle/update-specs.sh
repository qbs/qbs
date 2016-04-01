#!/bin/bash
# Update build specs from Xcode - this script should be run when new Xcode releases are made.
specs_dir="$(xcrun --sdk macosx --show-sdk-platform-path)/Developer/Library/Xcode/Specifications"
spec_files=("MacOSX Package Types.xcspec" "MacOSX Product Types.xcspec")
for spec_file in "${spec_files[@]}" ; do
    printf "%s\n" "$(plutil -convert json -r -o - "$specs_dir/$spec_file")" > "${spec_file// /-}"
done
xcode_version="$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' \
    "$(xcode-select --print-path)/../Info.plist")"
echo "Updated build specs from Xcode $xcode_version"
