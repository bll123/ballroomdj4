#!/bin/bash

stype=$(uname -s)

case $stype in
  Darwin)
    install_name_tool -change '../bin/libbdj4.dylib' \
        '@executable_path/../bin/libbdj4.dylib' \
        $@
    install_name_tool -change '../bin/libbdj4common.dylib' \
        '@executable_path/../bin/libbdj4common.dylib' \
        $@
    # libvlc is handled with DYLD_FALLBACK_PATH
    ;;
  Linux)
    patchelf --set-rpath '$ORIGIN/../bin' $@
    ;;
  *BSD)
    ;;
  MINGW*|MSYS*|CYGWIN*)
    ICON=bdj4_icon.ico
    rm -f $ICON launcher.rc launcer.res
    ;;
esac