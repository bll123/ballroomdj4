#!/bin/bash

for f in dances.txt \
    ; do
  echo "=== $f"
  diff -u ../data/${f}.bak.1 ../data/${f}
done
