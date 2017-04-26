#!/bin/bash
pip install -U --isolated --prefix=$PWD --no-binary :all: --no-compile --no-deps biplist dmgbuild ds_store mac_alias
rm lib/python2.7/site-packages/dmgbuild/resources/*.tiff
