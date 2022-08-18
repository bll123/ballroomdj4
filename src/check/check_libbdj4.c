#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <locale.h>

#pragma clang diagnostic push
#pragma gcc diagnostic push
#pragma clang diagnostic ignored "-Wformat-extra-args"
#pragma gcc diagnostic ignored "-Wformat-extra-args"

#include <check.h>

#include "check_bdj.h"

void
check_libbdj4 (SRunner *sr)
{
  Suite   *s;

  /* libbdj4
   *  bdjvarsdf
   *    needed by tests
   *  bdjvarsdfload
   *    needed by tests
   *  templateutil
   *    needed by tests
   *  dnctypes              complete
   *  dance                 complete
   *  genre                 complete
   *  level                 complete
   *  rating                complete
   *  songfav               complete
   *  status                complete
   *  songutil              complete
   *  tagdef                complete
   *  song                  complete
   *  musicdb               complete
   *  songlist
   *  autosel
   *  songfilter
   *  dancesel
   *  sequence
   *  songsel
   *  playlist
   *  sortopt
   *  dispsel
   *  orgutil               partial
   *  validate              partial
   *  webclient
   *  audiotag
   *  m3u
   *  orgopt
   *  bdj4init
   *  msgparse
   *  songdb
   *  volreg
   *  musicq
   */

  s = dnctypes_suite();
  srunner_add_suite (sr, s);

  s = dance_suite();
  srunner_add_suite (sr, s);

  s = genre_suite();
  srunner_add_suite (sr, s);

  s = rating_suite();
  srunner_add_suite (sr, s);

  s = level_suite();
  srunner_add_suite (sr, s);

  s = songfav_suite();
  srunner_add_suite (sr, s);

  s = status_suite();
  srunner_add_suite (sr, s);

  s = songutil_suite();
  srunner_add_suite (sr, s);

  s = tagdef_suite();
  srunner_add_suite (sr, s);

  s = song_suite();
  srunner_add_suite (sr, s);

  s = musicdb_suite();
  srunner_add_suite (sr, s);

  s = orgutil_suite();
  srunner_add_suite (sr, s);

  s = validate_suite();
  srunner_add_suite (sr, s);
}
