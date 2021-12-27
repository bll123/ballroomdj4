#include "config.h"

#include <stdlib.h>
#include <check.h>

#include "check_bdj.h"
#include "sysvars.h"
#include "tagdef.h"

int
main (void)
{
  int     number_failed;
  Suite   *s;
  SRunner *sr;

  sysvarsInit ();
  tagdefInit ();

  s = tmutil_suite();
  sr = srunner_create (s);
  s = fileutil_suite();
  srunner_add_suite (sr, s);
  s = process_suite();
  srunner_add_suite (sr, s);
  s = lock_suite();
  srunner_add_suite (sr, s);
  s = list_suite();
  srunner_add_suite (sr, s);
  s = datafile_suite();
  srunner_add_suite (sr, s);
  s = rafile_suite();
  srunner_add_suite (sr, s);
  s = sock_suite();
  srunner_add_suite (sr, s);
  srunner_run_all (sr, CK_ENV);
  number_failed = srunner_ntests_failed (sr);
  srunner_free (sr);

  tagdefCleanup ();

  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
