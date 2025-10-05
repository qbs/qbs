#!/bin/bash

SOURCE_DIR=$1
if [[ -z "${SOURCE_DIR}" ]]; then
   echo "Usage: ./sync path/to/quickjs/"
   exit 1
fi;

TARGET_DIR=$(dirname "$0")

FILES="
builtin-array-fromasync.h
cutils.c
cutils.h
dtoa.c
dtoa.h
libregexp-opcode.h
libregexp.c
libregexp.h
libunicode-table.h
libunicode.c
libunicode.h
LICENSE
list.h
quickjs-atom.h
quickjs-c-atomics.h
quickjs-opcode.h
quickjs.c
quickjs.h
"

for FILE in $FILES
do
    echo "$SOURCE_DIR/$FILE -> $TARGET_DIR/$FILE"
    cp "$SOURCE_DIR/$FILE" "$TARGET_DIR/$FILE"
done
