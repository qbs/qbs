#!/bin/bash
repos=(biplist.git@v1.0.2 dmgbuild.git@v1.3.1 ds_store@v1.1.2 mac_alias.git@v2.0.6)
for repo in "${repos[@]}" ; do
    pip install -U --isolated "--prefix=$PWD" --no-binary :all: --no-compile --no-deps \
        "git+git://github.com/qbs/$repo"
done
rm lib/python2.7/site-packages/dmgbuild/resources/*.tiff
