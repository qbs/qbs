#!/bin/bash

SOURCE_DIR=$1
if [[ -z "${SOURCE_DIR}" ]]; then
   echo "Usage: ./sync path/to/quickjs/"
   exit 1
fi;

TARGET_DIR=$(dirname "$0")

FILES="
LICENSE
cutils.h
libbf.h
libregexp.h
libunicode.c
libunicode-table.h
list.h
quickjs.c
quickjs.h
cutils.c
libbf.c
libregexp.c
libregexp-opcode.h
libunicode.h
quickjs-atom.h
quickjs-c-atomics.h
quickjs-opcode.h"

for FILE in $FILES
do
    echo "$SOURCE_DIR/$FILE -> $TARGET_DIR/$FILE"
    cp "$SOURCE_DIR/$FILE" "$TARGET_DIR/$FILE"
done