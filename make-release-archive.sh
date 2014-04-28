#!/bin/sh

test $# -eq 2 || { echo "Usage: $(basename $0) <archive format> <tag>" >&2; exit 1; }

format=${1}
tag=${2}
version=${tag#v}

git archive --format=${format} --prefix=qbs-${version}/ -o qbs-${version}.src.${format} ${tag}
