#!/bin/bash

cwd=$(pwd)
case ${cwd} in
  */src)
    cd po
    ;;
  */bdj4)
    cd src/po
    ;;
esac

function mksub {
  tmpl=$1
  tempf=$2
  lang=$3
  pofile=$4

  set -o noglob
  echo "-- Processing $tmpl"
  sedcmd=""
  while read -r line; do
    nl=$(echo $line |
      sed -e 's,^\.\.,,' -e 's,^,msgid ",' -e 's,$,",')
    xl=$(sed -n "\~^${nl}$~ {n;p}" $pofile)
    case $xl in
      ""|msgstr\ \"\")
        continue
        ;;
    esac
    xl=$(echo $xl | sed -e 's,^msgstr ",,' -e 's,"$,,')
    case $line in
      ..*)
        xl=$(echo "..$xl")
        ;;
    esac
    sedcmd+="-e '\~^${line}\$~ s~.*~${xl}~' "
  done < $tempf

  eval sed ${sedcmd} $tmpl > $tmpl.$lang
  set +o noglob
}

function mkhtmlsub {
  tmpl=$1
  tempf=$2
  lang=$3
  pofile=$4

  set -o noglob
  echo "-- Processing $tmpl"
  sedcmd=""
  while read -r line; do
    nl=$line
    case $nl in
      "")
        continue
        ;;
    esac
    xl=$(sed -n "\~msgid .${nl}.~ {n;p}" $pofile)
    case $xl in
      ""|msgstr\ \"\")
        continue
        ;;
    esac
    xl=$(echo $xl | sed -e 's,^msgstr ",,' -e 's,"$,,')
    sedcmd+="-e '\~value=\"${nl}\"~ s~value=\"${nl}\"~value=\"${xl}\"~' "
  done < $tempf

  eval sed ${sedcmd} $tmpl > $tmpl.$lang
  set +o noglob
}

function mkimgsub {
  tmpl=$1
  tempf=$2
  lang=$3
  pofile=$4

  set -o noglob
  echo "-- Processing $tmpl"
  sedcmd=""
  while read -r line; do
    nl=$line
    case $nl in
      "")
        continue
        ;;
    esac
    xl=$(sed -n "\~msgid .${nl}.~ {n;p}" $pofile)
    case $xl in
      ""|msgstr\ \"\")
        continue
        ;;
    esac
    xl=$(echo $xl | sed -e 's,^msgstr ",,' -e 's,"$,,')
    sedcmd+="-e '\~aria-label=\"${nl}\"~ s~aria-label=\"${nl}\"~aria-label=\"${xl}\"~' "
  done < $tempf

  eval sed ${sedcmd} $tmpl > $tmpl.$lang
  set +o noglob
}

TMP=temp.txt
CTMP=tempcomp.txt

echo "-- Creating .mo files"
for i in *.po; do
  j=$(echo $i | sed 's,.po$,,')
  mkdir -p ../../locale/$j/LC_MESSAGES
  msgfmt -c -o ../../locale/$j/LC_MESSAGES/bdj4.mo $i
  # for the time being, there is no en_GB.po yet.
  if [[ $i == en_GB.po ]]; then
    j=en
    mkdir -p ../../locale/$j/LC_MESSAGES
    msgfmt -c -o ../../locale/$j/LC_MESSAGES/bdj4.mo $i
  fi
done

> $CTMP

for pofile in $(cat complete.txt); do

  lang=$(echo $pofile | sed 's,\.po$,,')

  desc=$(sed -n -e '1,1p' $pofile)
  desc=$(echo $desc | sed -e 's/^# == //')
  echo $desc >> $CTMP
  echo "..$lang" >> $CTMP

  case $pofile in
    en*)
      continue
      ;;
  esac

  fn=../../templates/dancetypes.txt
  sed -e '/^#/d' $fn > $TMP
  mksub $fn $TMP $lang $pofile

  fn=../../templates/dances.txt
  sed -n -e '/^DANCE/ {n;p}' $fn > $TMP
  sed -n -e '/^TYPE/ {n;p}' $fn >> $TMP
  sed -n -e '/^SPEED/ {n;p}' $fn >> $TMP
  sort -u $TMP > $TMP.n
  mv -f $TMP.n $TMP
  mksub $fn $TMP $lang $pofile

  fn=../../templates/ratings.txt
  sed -n -e '/^RATING/ {n;p}' $fn > $TMP
  mksub $fn $TMP $lang $pofile

  fn=../../templates/genres.txt
  sed -n -e '/^GENRE/ {n;p}' $fn > $TMP
  mksub $fn $TMP $lang $pofile

  fn=../../templates/levels.txt
  sed -n -e '/^LABEL/ {n;p}' $fn > $TMP
  mksub $fn $TMP $lang $pofile

  fn=../../templates/status.txt
  sed -n -e '/^STATUS/ {n;p}' $fn > $TMP
  mksub $fn $TMP $lang $pofile

  for fn in ../../templates/*.pldances; do
    sed -n -e '/^DANCE/ {n;p}' $fn > $TMP
    sort -u $TMP > $TMP.n
    mv -f $TMP.n $TMP
    mksub $fn $TMP $lang $pofile
  done

  for fn in ../../templates/*.pl; do
    sed -n -e '/^DANCERATING/ {n;p}' $fn > $TMP
    sed -n -e '/^DANCELEVEL/ {n;p}' $fn >> $TMP
    sort -u $TMP > $TMP.n
    mv -f $TMP.n $TMP
    mksub $fn $TMP $lang $pofile
  done

  for fn in ../../templates/*.sequence; do
    sed -e '/^#/d' $fn > $TMP
    mksub $fn $TMP $lang $pofile
  done

  for fn in ../../templates/*.html; do
    case $fn in
      *qrcode.html)
        continue
        ;;
      *mobilemq.html)
        # mobilemq has only the title string 'marquee'.
        # handle this later; nl doesn't change it
        continue
        ;;
    esac
    egrep 'value=' $fn |
        sed -e 's,.*value=",,' -e 's,".*,,' -e '/^100$/ d' > $TMP
    sort -u $TMP > $TMP.n
    mv -f $TMP.n $TMP
    mkhtmlsub $fn $TMP $lang $pofile
  done

  fn=../../templates/fades.svg
  egrep 'aria-label=' $fn |
      sed -e 's,.*aria-label=",,' -e 's,".*,,' > $TMP
  sort -u $TMP > $TMP.n
  mv -f $TMP.n $TMP
  mkimgsub $fn $TMP $lang $pofile

  if [[ $lang == nl ]]; then
    cwd=$(pwd)
    cd ../../templates
    mv -f automatic.pl.nl Automatisch.pl.nl
    mv -f automatic.pldances.nl Automatisch.pldances.nl
    mv -f standardrounds.pl.nl Standaardrondes.pl.nl
    sed -e 's/^..standardrounds/..Standaardrondes/' Standaardrondes.pl.nl > $TMP
    mv -f $TMP Standaardrondes.pl.nl
    mv -f standardrounds.pldances.nl Standaardrondes.pldances.nl
    mv -f standardrounds.sequence.nl Standaardrondes.sequence.nl
    for fn in *.html.nl; do
      case $fn in
        mobilemq.html|qrcode.html)
          continue
          ;;
      esac
      sed -e 's/English/Nederlands/' $fn > $fn.n
      mv -f $fn.n $fn
    done
    cd $cwd
  fi
done

mv -f $CTMP ../../locale/locales.txt

test -f $TMP && rm -f $TMP
test -f $CTMP && rm -f $CTMP
exit 0
