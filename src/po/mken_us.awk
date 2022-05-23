#!/usr/bin/awk

function convert (l) {
  gsub (/== English .GB/, "== English (US", l);
  gsub (/english\/gb/, "english/us", l);
  l = gensub (/([Cc])olour/, "\\1olor", "g", l);
  l = gensub (/([Oo])rganis/, "\\1rganiz", "g", l);
  l = gensub (/([Cc])entimetre/, "\\1entimeter", "g", l);
  gsub (/LICENCE/, "LICENSE", l);
  return l;
}

BEGIN { chg = 0; }

{
  ol = $0;

  if ($0 ~ /^msgid/) {
    tl = convert(ol);
    if (tl != ol) {
      chg = 1;
    }
    l = ol;
  } else {
    l = convert(ol);
  }

  if ($0 ~ /^msgstr/) {
    if (chg) {
      sub (/^msgid/, "msgstr", tl);
      l = tl;
      chg = 0;
    }
  }

  print l;
}

