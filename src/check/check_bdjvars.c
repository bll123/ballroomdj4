#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma gcc diagnostic push
#pragma clang diagnostic ignored "-Wformat-extra-args"
#pragma gcc diagnostic ignored "-Wformat-extra-args"

#include <check.h>

#include "bdjvars.h"
#include "check_bdj.h"
#include "log.h"
#include "sysvars.h"

START_TEST(bdjvars_init)
{
  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- bdjvars_init");

  ck_assert_int_eq (bdjvarsIsInitialized (), 0);
  bdjvarsInit ();
  ck_assert_int_eq (sysvarsGetNum (SVL_BASEPORT) +
      bdjvarsGetNum (BDJVL_NUM_PORTS) * sysvarsGetNum (SVL_BDJIDX),
      bdjvarsGetNum (BDJVL_MAIN_PORT));
  ck_assert_int_eq (bdjvarsIsInitialized (), 1);
  bdjvarsCleanup ();
  ck_assert_int_eq (bdjvarsIsInitialized (), 0);
}
END_TEST

START_TEST(bdjvars_init_idx)
{

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- bdjvars_init_idx");

  sysvarsSetNum (SVL_BDJIDX, 2);
  bdjvarsInit ();
  ck_assert_int_eq (sysvarsGetNum (SVL_BASEPORT) +
      bdjvarsGetNum (BDJVL_NUM_PORTS) * sysvarsGetNum (SVL_BDJIDX),
      bdjvarsGetNum (BDJVL_MAIN_PORT));
  bdjvarsCleanup ();
}
END_TEST

START_TEST(bdjvars_adjust)
{

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- bdjvars_adjust");

  bdjvarsInit ();
  sysvarsSetNum (SVL_BDJIDX, 3);
  bdjvarsAdjustPorts ();
  ck_assert_int_eq (sysvarsGetNum (SVL_BASEPORT) +
      bdjvarsGetNum (BDJVL_NUM_PORTS) * sysvarsGetNum (SVL_BDJIDX),
      bdjvarsGetNum (BDJVL_MAIN_PORT));
  bdjvarsCleanup ();
}
END_TEST

START_TEST(bdjvars_set)
{

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- bdjvars_set");

  bdjvarsInit ();
  bdjvarsSetNum (BDJVL_BPM_COUNTER_PORT, 12);
  ck_assert_int_eq (bdjvarsGetNum (BDJVL_BPM_COUNTER_PORT), 12);
  bdjvarsSetStr (BDJV_DB_TOP_DIR, "test");
  ck_assert_str_eq (bdjvarsGetStr (BDJV_DB_TOP_DIR), "test");
  bdjvarsCleanup ();
}
END_TEST

Suite *
bdjvars_suite (void)
{
  Suite     *s;
  TCase     *tc;

  s = suite_create ("bdjvars");
  tc = tcase_create ("bdjvars");
  tcase_set_tags (tc, "libcommon");
  tcase_add_test (tc, bdjvars_init);
  tcase_add_test (tc, bdjvars_init_idx);
  tcase_add_test (tc, bdjvars_adjust);
  tcase_add_test (tc, bdjvars_set);
  suite_add_tcase (s, tc);
  return s;
}

