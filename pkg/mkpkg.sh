#!/bin/bash

pkgname=BallroomDJ4
spkgnm=bdj4
instdir=bdj4-install
# macos install needs these
typeset -u pkgnameuc
pkgnameuc=${pkgname}
typeset -l pkgnamelc
pkgnamelc=${pkgname}

function copysrcfiles (
  systype=$1
  stage=$2

  filelist="LICENSE.txt README.txt VERSION.txt
      packages/mongoose/mongoose.[ch]"
  # the http directory will be created
  dirlist="src conv img install licenses locale pkg templates web wiki \
      conv img install licenses locale templates"

  case ${systype} in
    Darwin)
      dirlist="$dirlist plocal/bin plocal/share/themes/macOS* plocal/share/icons"
      ;;
    MSYS*|MINGW*)
      dirlist="$dirlist plocal/bin plocal/share/themes/Wind* plocal/share/icons"
      ;;
  esac

  echo "-- copying files to $stage"
  for f in $filelist; do
    dir=$(dirname ${f})
    test -d ${stage}/${dir} || mkdir -p ${stage}/${dir}
    cp -pf ${f} ${stage}/${dir}
  done
  for d in $dirlist; do
    cp -pr ${d} ${stage}
  done

  echo "   removing exclusions"
  test -d ${stage}/src/build && rm -rf ${stage}/src/build
)

function copyreleasefiles (
  systype=$1
  stage=$2

  filelist="LICENSE.txt README.txt VERSION.txt"
  dirlist="bin conv img install licenses locale templates"

  case ${systype} in
    Darwin)
      dirlist="$dirlist plocal/bin plocal/share/themes/macOS* plocal/share/icons"
      ;;
    MSYS*|MINGW*)
      dirlist="$dirlist plocal/bin plocal/share/themes/Wind* plocal/share/icons"
      ;;
  esac

  echo "-- copying files to $stage"
  for f in $filelist; do
    dir=$(dirname ${f})
    test -d ${stage}/${dir} || mkdir -p ${stage}/${dir}
    cp -pf ${f} ${stage}/${dir}
  done
  for d in $dirlist; do
    cp -pr ${d} ${stage}
  done

  echo "   removing exclusions"
  rm -f \
      ${stage}/bin/bdj4cli \
      ${stage}/bin/bdj4se \
      ${stage}/bin/check_all \
      ${stage}/bin/chkprocess \
      ${stage}/img/mkicon.sh \
      ${stage}/img/README.txt \
      ${stage}/plocal/bin/checkmk \
      ${stage}/plocal/bin/curl-config \
      ${stage}/plocal/bin/libcheck-*.dll \
      ${stage}/plocal/bin/ocspcheck.exe \
      ${stage}/plocal/bin/openssl.exe
)

# setup

cwd=$(pwd)
case $cwd in
  */bdj4)
    ;;
  */bdj4/*)
    cwd=$(dirname $cwd)
    while : ; do
      case $cwd in
        */bdj4)
          break
          ;;
      esac
      cwd=$(dirname $cwd)
    done
    cd $cwd
    ;;
esac

systype=$(uname -s)
case $systype in
  Linux)
    tag=linux
    platform=unix
    sfx=.run
    ;;
  Darwin)
    tag=macos
    platform=unix
    sfx=.run
    ;;
  MINGW64*)
    tag=win64
    platform=windows
    sfx=.exe
    ;;
  MINGW32*)
    tag=win32
    platform=windows
    sfx=.exe
    ;;
esac

echo "-- checking"

instpfx=install/install-prefix.sh
sza=$(stat --format '%s' ${instpfx})
szb=$(grep '^size=' ${instpfx} | sed -e 's/size=//')
if [[ $sza -ne $szb ]]; then
  echo "The ${instpfx} file has the wrong size set."
  exit 1
fi

echo "-- copying licenses"
licdir=licenses
rm -f ${licdir}/*
cp -f packages/mongoose/LICENSE ${licdir}/mongoose.LICENSE
if [[ $tag == windows ]]; then
  cp -f packages/libressl*/COPYING ${licdir}/libressl.LICENSE
  cp -f packages/c-ares*/LICENSE.md ${licdir}/c-ares.LICENSE.md
  cp -f packages/curl*/COPYING ${licdir}/curl.LICENSE
  cp -f packages/nghttp*/LICENSE ${licdir}/nghttp2.LICENSE
  cp -f packages/zstd*/LICENSE ${licdir}/zstd.LICENSE
fi

# create manifests

# case $systype in
#   Linux)
#       echo "-- creating source manifest"
#       (cd src; make distclean > /dev/null 2>&1)
#     ;;
# esac

#echo "-- building software"
#(
#  cd src
#  make distclean
#  make > ../tmp/pkg-build.log 2>&1
#  make tclean > /dev/null 2>&1
#)

(cd src; make tclean > /dev/null 2>&1)

# on windows, copy all of the required .dll files to plocal/bin
# this must be done after the build and before the manifest is created.

if [[ $platform == windows ]]; then

  # bll@win10-64 MINGW64 ~/bdj4/bin
  # $ ldd bdj4.exe | grep mingw
  #       libintl-8.dll => /mingw64/bin/libintl-8.dll (0x7ff88c570000)
  #       libiconv-2.dll => /mingw64/bin/libiconv-2.dll (0x7ff8837e0000)

  echo "-- copying .dll files"
  dlllistfn=tmp/dll-list.txt
  > $dlllistfn

  for fn in bin/*.exe /mingw64/bin/gdbus.exe ; do
    ldd $fn |
      grep mingw |
      sed -e 's,.*=> ,,' -e 's,\.dll .*,.dll,' >> $dlllistfn
  done
  for fn in $(sort -u $dlllistfn); do
    cp -pf $fn plocal/bin
  done
  rm -f $dlllistfn

  # stage the other required gtk files.

  cp -f /mingw64/bin/gdbus.exe  plocal/bin

fi # is windows

echo "-- creating release manifest"
./pkg/mkmanifest.sh

# update build number

echo "-- updating build number"

. ./VERSION.txt
BUILD=$(($BUILD+1))
cat > VERSION.txt << _HERE_
VERSION=$VERSION
BUILD=$BUILD
RELEASELEVEL=$RELEASELEVEL
_HERE_

case $RELEASELEVEL in
  alpha|beta)
    rlstag=-$RELEASELEVEL
    ;;
  production)
    rlstag=""
    ;;
esac

# staging / create packags

case $systype in
  Linux)
    echo "-- create source package"
    stagedir=tmp/${spkgnm}-src
    test -d ${stagedir} && rm -rf ${stagedir}
    mkdir -p ${stagedir}
    nm=${spkgnm}-${VERSION}-src${rlstag}.tar.gz

    copysrcfiles ${systype} ${stagedir}
    (cd tmp;tar -c -z -f - $(basename $stagedir)) > ${nm}
    echo "## source package ${nm} created"
    rm -rf ${stagedir}
    ;;
esac

echo "-- create release package"

stagedir=tmp/${instdir}
test -d ${stagedir} && rm -rf ${stagedir}
mkdir -p ${stagedir}
manfn=install/manifest.txt
tmpnm=tmp/tfile.dat
tmpzip=tmp/tfile.zip
tmpsep=tmp/sep.txt
tmpmac=tmp/macos

nm=${spkgnm}-${VERSION}-installer-${tag}${rlstag}${sfx}

test -d ${stagedir} && rm -rf ${stagedir}
mkdir ${stagedir}

case $systype in
  Linux)
    copyreleasefiles ${systype} ${stagedir}
    echo "-- creating install package"
    (cd tmp;tar -c -z -f - $(basename $stagedir)) > ${tmpnm}
    cat install/install-prefix.sh ${tmpnm} > ${nm}
    rm -f ${tmpnm}
    ;;
  Darwin)
    mkdir -p ${stagedir}/Contents/MacOS
    mkdir -p ${stagedir}/Contents/Resources
    mkdir -p ${tmpmac}
    cp -f img/BallroomDJ4.icns ${stagedir}/Contents/Resources
    sed -e "s/#BDJVERSION#/${VERSION}/g" \
        -e "s/#BDJPKGNAME#/${pkgname}/g" \
        -e "s/#BDJUCPKGNAME#/${pkgnameuc}/g" \
        -e "s/#BDJLCPKGNAME#/S{pkgnamelc}/g" \
        pkg/macos/Info.plist \
        > ${stagedir}/Contents/MacOS/Info.plist
    echo -n 'BDJBDJ4#' > ${stagedir}/Contents/MacOS/PkgInfo
    copyreleasefiles ${systype} ${stagedir}/Contents/MacOS
    echo "-- creating install package"
    (cd tmp;tar -c -z -f - $(basename $stagedir)) > ${tmpnm}
    cat install/install-prefix.sh ${tmpnm} > ${nm}
    rm -f ${tmpnm}
    ;;
  MINGW64*|MINGW32*)
    copyreleasefiles ${systype} ${stagedir}
    echo "-- creating install package"
    (cd tmp; zip -r ../${tmpzip} $(basename $stagedir) ) > pkg-zip.log 2>&1
    echo -n '!~~BDJ4~~!' > ${tmpsep}
    cat bin/bdj4se.exe \
        ${tmpsep} \
        plocal/bin/miniunz.exe \
        ${tmpsep} \
        ${tmpzip} \
        > ${nm}
    rm -f ${tmpzip} ${tmpsep}
    ;;
esac
chmod a+rx ${nm}

echo "## release package ${nm} created"
rm -rf ${stagedir}

exit 0
