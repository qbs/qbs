#!/bin/sh

test $# -eq 1 || { echo "Error: Tag required." >&2; exit 1; }

tag=${1}
version=${tag#v}

git archive --format=tar.gz --prefix=qbs-${version}/ -o qbs-${version}.src.tar.gz ${tag}
