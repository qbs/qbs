#!/bin/bash

for file in $(find /cores -maxdepth 1 -name 'core.*' -print); do
    echo "================================ $file ================================"
    gdb -ex 'thread apply all bt' -ex 'quit' ./release/install-root/usr/local/bin/qbs $file
done;
