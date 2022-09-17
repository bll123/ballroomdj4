#!/bin/bash

for f in bdjconfig.txt profile00/bdjconfig.txt \
    bll-g7/bdjconfig.txt bll-g7/profile00/bdjconfig.txt \
    ; do
  echo "=== $f"
  diff -u ../data/${f}.bak.1 ../data/${f}
done
