#!/bin/bash

cwd=$(pwd)
case ${cwd} in
  */po)
    cd ..
    ;;
esac

rm -f po/*~ po/old/*~ > /dev/null 2>&1
declare -A helptext
export helptext
export helpkeys=""
export POTFILE=bdj4.pot

. ../VERSION.txt
export VERSION
dt=$(date '+%F')
export dt

function mkpo {
  lang=$1
  out=$2
  xlator=$3
  dlang=$4
  elang=$5

  echo "-- $(date +%T) creating $out"
  if [[ -f ${out} ]]; then
    mv ${out} ${out}.old
  fi
  > ${out}
  echo "# == $dlang" >> ${out}
  echo "# -- $elang" >> ${out}
  sed -e '1,6 d' \
      -e "s/YEAR-MO-DA.*ZONE/${dt}/" \
      -e "s/: [0-9][0-9][0-9 :-]*/: ${dt}/" \
      -e "s/PACKAGE/BDJ4/" \
      -e "s/VERSION/${VERSION}/" \
      -e "s/FULL NAME.*ADDRESS./${xlator}/" \
      -e "s/Bugs-To: /Bugs-To: brad.lanam.comp@gmail.com/" \
      -e "s/Language: /Language: ${lang}/" \
      -e "s,Language-Team:.*>,Language-Team: $elang," \
      -e "s/CHARSET/utf-8/" \
      bdj4.pot >> ${out}
}

function extractHelpData {
  fn=$1
  ctxt=$2
  TMP=$3

  intitle=F
  intextkey=F
  intext=F
  export intext

  while read -r line; do
    case $line in
      "")
        ;;
      KEY)
        if [[ $intext == T ]]; then
          helpkeys+=" $textkey"
          helptext[$textkey]=$(echo -e $text)
        fi
        intitle=F
        intextkey=F
        intext=F
        ;;
      TITLE)
        intitle=T
        ;;
      TEXT)
        intitle=F
        intextkey=T
        text="msgstr \"\"\n"
        ;;
      ..*)
        if [[ $intitle == T ]]; then
          echo "// CONTEXT: title of a getting started help section" >> $TMP
          echo $line >> $TMP
        fi
        if [[ $intextkey == T ]]; then
          echo "// CONTEXT: text-key for getting started help section (translate the text, not the key)" >> $TMP
          echo $line >> $TMP
          tline=$(echo $line | sed -e 's/^\.\.//')
          textkey=$tline
          intextkey=F
          intext=T
        fi
        ;;
      \#*)
        if [[ $intext == T ]]; then
          tline=$(echo $line | sed -e 's/^#//')
          text+="\"$tline\"\n"
        fi
        ;;
    esac
  done < $fn

  if [[ $intext == T ]]; then
    helpkeys+=" $textkey"
    helptext[$textkey]=$(echo -e $text)
  fi
}

function updateHelpText {
  fn=$1

  # update the help text
  for hk in $helpkeys; do
    awk -v HK=$hk -v TEXT="${helptext[$hk]}" -f set-helptext.awk ${fn} \
      > ${fn}.n
    mv -f ${fn}.n ${fn}
  done
}


TMP=potemplates.c

> $TMP

ctxt="// CONTEXT: dance type"
fn=../templates/dancetypes.txt
sed -e '/^#/d' -e 's,^,..,' -e "s,^,${ctxt}\n," $fn >> $TMP

ctxt="// CONTEXT: dance"
fn=../templates/dances.txt
sed -n -e "/^DANCE/ {n;s,^,${ctxt}\n,;p}" $fn >> $TMP

ctxt="// CONTEXT: rating"
fn=../templates/ratings.txt
sed -n -e "/^RATING/ {n;s,^,${ctxt}\n,;p}" $fn >> $TMP

ctxt="// CONTEXT: genre"
fn=../templates/genres.txt
sed -n -e "/^GENRE/ {n;s,^,${ctxt}\n,;p}" $fn >> $TMP

ctxt="// CONTEXT: dance level"
fn=../templates/levels.txt
sed -n -e "/^LEVEL/ {n;s,^,${ctxt}\n,;p}" $fn >> $TMP

ctxt="// CONTEXT: status"
fn=../templates/status.txt
sed -n -e "/^STATUS/ {n;s,^,${ctxt}\n,;p}" $fn >> $TMP

fn=../templates/bdjconfig.txt.p
ctxt="// CONTEXT: name of a music queue"
sed -n -e "/^QUEUE_NAME_[AB]/ {n;s,^,${ctxt}\n,;p}" $fn >> $TMP
echo "// CONTEXT: The completion message displayed on the marquee when the playlist is finished." >> $TMP
sed -n -e '/^COMPLETEMSG/ {n;p}' $fn >> $TMP

ctxt="// CONTEXT: title for a section in the getting started helper "
fn=../templates/helpdata.txt
extractHelpData $fn "$ctxt" $TMP

ctxt="// CONTEXT: text from the HTML templates (buttons and labels)"
egrep 'value=' ../templates/*.html |
  sed -e 's,.*value=",,' -e 's,".*,,' -e '/^100$/ d' -e 's,^,..,' \
      -e "s,^,${ctxt}\n," >> $TMP

# names of playlist files
echo "// CONTEXT: The name of the 'automatic' playlist file" >> $TMP
echo "..automatic" >> $TMP
echo "// CONTEXT: The name of the 'standardrounds' playlist file" >> $TMP
echo "..standardrounds" >> $TMP
echo "// CONTEXT: The name of the 'queuedance' playlist file" >> $TMP
echo "..QueueDance" >> $TMP
echo "// CONTEXT: The name of the 'raffle songs' playlist file" >> $TMP
echo "..Raffle Songs" >> $TMP

# linux desktop shortcut
echo "// CONTEXT: tooltip for desktop icon" >> $TMP
egrep '^Comment=' ../install/bdj4.desktop |
  sed -e 's,Comment=,,' -e 's,^,..,' >> $TMP

sed -e '/^\.\./ {s,^\.\.,, ; s,^,_(", ; s,$,"),}' $TMP > $TMP.n
mv -f $TMP.n $TMP

echo "-- $(date +%T) extracting"
xgettext -s -d bdj4 \
    --language=C \
    --add-comments=CONTEXT: \
    --no-location \
    --keyword=_ \
    --flag=_:1:pass-c-format \
    *.c */*.c \
    -p po -o ${POTFILE}

rm -f $TMP

cd po

updateHelpText ${POTFILE}

mkpo en en_GB.po 'Automatically generated' 'English (GB)' english/gb
rm -f en_GB.po.old

mkpo nl nl_BE.po 'marimo' Nederlands dutch

# mkpo de de_DE.po 'unassigned' Deutsch german
# updateHelpText de_DE.po

echo "-- $(date +%T) updating translations from old .po files"
./lang-lookup.sh
echo "-- $(date +%T) creating english/us .po files"
awk -f mken_us.awk en_GB.po > en_US.po
echo "-- $(date +%T) finished"

exit 0
