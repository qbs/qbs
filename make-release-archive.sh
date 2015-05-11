#!/bin/sh

test $# -eq 2 || { echo "Usage: $(basename $0) <archive format> <tag>" >&2; exit 1; }

format=${1}
tag=${2}
version=${tag#v}
dir_name=qbs-src-${version}

git archive --format=${format} --prefix=${dir_name}/ -o ${dir_name}.${format} ${tag}
