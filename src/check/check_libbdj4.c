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

int
check_libbdj4 (bool skiplong)
{
  int     number_failed = 0;
  Suite   *s;
  SRunner *sr;

  /* libbdj4
   */

  fprintf (stdout, "=== libbdj4\n");

  s = orgutil_suite();
  sr = srunner_create (s);

  s = validate_suite();
  srunner_add_suite (sr, s);

  srunner_run_all (sr, CK_ENV);
  number_failed += srunner_ntests_failed (sr);
  srunner_free (sr);

  return number_failed;
}
