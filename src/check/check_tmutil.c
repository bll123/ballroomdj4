#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

#pragma clang diagnostic push
#pragma gcc diagnostic push
#pragma clang diagnostic ignored "-Wformat-extra-args"
#pragma gcc diagnostic ignored "-Wformat-extra-args"

#include <check.h>

#include "tmutil.h"
#include "check_bdj.h"
#include "sysvars.h"

START_TEST(mstime_chk)
{
  time_t      tm_s;
  time_t      tm_chk;

  tm_s = time (NULL);
  tm_s *= 1000;
  tm_chk = mstime ();
  /* the granularity of the time() call is high */
  ck_assert_int_lt (tm_chk - tm_s, 1000);
  tm_s = mstime ();
  tm_chk = mstime ();
  ck_assert_int_lt (tm_chk - tm_s, 10);
}
END_TEST

START_TEST(mssleep_chk)
{
  time_t      tm_s;
  time_t      tm_e;

  tm_s = mstime ();
  mssleep (2000);
  tm_e = mstime ();
  ck_assert_int_lt (tm_e - tm_s - 2000, 30);
}
END_TEST

START_TEST(mstimestartofday_chk)
{
  time_t    tm_s;
  time_t    tm_chk;

  tm_s = mstime ();
  tm_chk = mstimestartofday ();
  /* this needs a test */
  ck_assert_int_ge (tm_s, tm_chk);
}
END_TEST

START_TEST(mstime_start_end)
{
  time_t      tm_s;
  time_t      tm_e;
  mstime_t    mi;
  time_t      m, d;

  tm_s = mstime ();
  mstimestart (&mi);
  mssleep (2000);
  m = mstimeend (&mi);
  tm_e = mstime ();
  d = tm_e - tm_s;
  d -= m;
  if (d < 0) {
    d = - d;
  }
  ck_assert_int_lt (d, 30);
}
END_TEST

START_TEST(mstime_set)
{
  mstime_t    tmset;
  time_t      tm_s;
  time_t      tm_e;
  time_t      m, d;

  tm_s = mstime ();
  mstimeset (&tmset, 2000);
  mssleep (2000);
  m = mstimeend (&tmset);
  tm_e = mstime ();
  d = tm_e - tm_s;
  d -= m;
  d -= 2000;
  if (d < 0) {
    d = - d;
  }
  ck_assert_int_lt (d, 30);
}
END_TEST

START_TEST(mstime_set_tm)
{
  mstime_t    tmset;
  time_t      m, s, d;

  s = mstime ();
  /* sets tmset to the value supplied */
  mstimesettm (&tmset, 2000);
  m = mstimeend (&tmset);
  d = s - m - 2000;
  if (d < 0) {
    d = -d;
  }
  ck_assert_int_lt (d, 30);
}
END_TEST

START_TEST(mstime_check)
{
  mstime_t    tmset;
  bool        rc;

  mstimeset (&tmset, 2000);
  rc = mstimeCheck (&tmset);
  ck_assert_int_eq (rc, false);
  mssleep (1000);
  rc = mstimeCheck (&tmset);
  ck_assert_int_eq (rc, false);
  mssleep (1000);
  rc = mstimeCheck (&tmset);
  ck_assert_int_eq (rc, true);
}
END_TEST

START_TEST(tmutildstamp_chk)
{
  char        buff [80];

  tmutilDstamp (buff, sizeof (buff));
  ck_assert_int_eq (strlen (buff), 10);
}
END_TEST

START_TEST(tmutildisp_chk)
{
  char        buff [80];

  tmutilDisp (buff, sizeof (buff));
  ck_assert_int_ge (strlen (buff), 16);
}
END_TEST

START_TEST(tmutiltstamp_chk)
{
  char        buff [80];

  tmutilTstamp (buff, sizeof (buff));
  ck_assert_int_eq (strlen (buff), 12);
}
END_TEST

START_TEST(tmutilshorttstamp_chk)
{
  char        buff [80];

  tmutilShortTstamp (buff, sizeof (buff));
  ck_assert_int_eq (strlen (buff), 4);
}
END_TEST

START_TEST(tmutiltoms_chk)
{
  char        buff [80];

  tmutilToMS (0, buff, sizeof (buff));
  ck_assert_str_eq (buff, " 0:00");
  tmutilToMS (59000, buff, sizeof (buff));
  ck_assert_str_eq (buff, " 0:59");
  tmutilToMS (60000, buff, sizeof (buff));
  ck_assert_str_eq (buff, " 1:00");
  tmutilToMS (119000, buff, sizeof (buff));
  ck_assert_str_eq (buff, " 1:59");
}
END_TEST

START_TEST(tmutiltomsd_chk)
{
  char        buff [80];
  char        tmp [80];

  tmutilToMSD (0, buff, sizeof (buff));
  snprintf (tmp, sizeof (tmp), "0:00%s000", sysvarsGetStr (SV_LOCALE_RADIX));
  ck_assert_str_eq (buff, tmp);
  tmutilToMSD (59001, buff, sizeof (buff));
  snprintf (tmp, sizeof (tmp), "0:59%s001", sysvarsGetStr (SV_LOCALE_RADIX));
  ck_assert_str_eq (buff, tmp);
  tmutilToMSD (60002, buff, sizeof (buff));
  snprintf (tmp, sizeof (tmp), "1:00%s002", sysvarsGetStr (SV_LOCALE_RADIX));
  ck_assert_str_eq (buff, tmp);
  tmutilToMSD (119003, buff, sizeof (buff));
  snprintf (tmp, sizeof (tmp), "1:59%s003", sysvarsGetStr (SV_LOCALE_RADIX));
  ck_assert_str_eq (buff, tmp);
}
END_TEST

START_TEST(tmutiltodatehm_chk)
{
  char        buff [80];

  tmutilToDateHM (mstime(), buff, sizeof (buff));
  ck_assert_int_ge (strlen (buff), 14);
}
END_TEST

START_TEST(tmutil_strtoms)
{
  long  value;

  value = tmutilStrToMS ("0:00");
  ck_assert_int_eq (value, 0);
  value = tmutilStrToMS ("1:00");
  ck_assert_int_eq (value, 60000);
  value = tmutilStrToMS ("2:00");
  ck_assert_int_eq (value, 120000);
  value = tmutilStrToMS ("1:59");
  ck_assert_int_eq (value, 119000);
  value = tmutilStrToMS ("1:01.005");
  ck_assert_int_eq (value, 61005);
  value = tmutilStrToMS ("1:02,005");
  ck_assert_int_eq (value, 62005);
}
END_TEST

START_TEST(tmutil_strtohm)
{
  long  value;

  value = tmutilStrToHM ("0:00");
  ck_assert_int_eq (value, 0);
  value = tmutilStrToHM ("12:00");
  ck_assert_int_eq (value, 720000);
  value = tmutilStrToHM ("12:00a");
  ck_assert_int_eq (value, 1440000);
  value = tmutilStrToHM ("12a");
  ck_assert_int_eq (value, 1440000);
  value = tmutilStrToHM ("1:45");
  ck_assert_int_eq (value, 105000);
  value = tmutilStrToHM ("12p");
  ck_assert_int_eq (value, 720000);
  value = tmutilStrToHM ("12:30p");
  ck_assert_int_eq (value, 750000);
  value = tmutilStrToHM ("1:30p");
  ck_assert_int_eq (value, 810000);
  value = tmutilStrToHM ("12:00p");
  ck_assert_int_eq (value, 720000);
}
END_TEST

Suite *
tmutil_suite (void)
{
  Suite     *s;
  TCase     *tc;

  s = suite_create ("tmutil");
  tc = tcase_create ("tmutil-base");
  tcase_add_test (tc, mstime_chk);
  suite_add_tcase (s, tc);
  tc = tcase_create ("tmutil-timers");
  tcase_set_tags (tc, "slow");
  tcase_add_test (tc, mssleep_chk);
  tcase_add_test (tc, mstimestartofday_chk);
  tcase_add_test (tc, mstime_start_end);
  tcase_add_test (tc, mstime_set);
  tcase_add_test (tc, mstime_set_tm);
  tcase_add_test (tc, mstime_check);
  suite_add_tcase (s, tc);
  tc = tcase_create ("tmutil-disp");
  tcase_add_test (tc, tmutildstamp_chk);
  tcase_add_test (tc, tmutildisp_chk);
  tcase_add_test (tc, tmutiltstamp_chk);
  tcase_add_test (tc, tmutilshorttstamp_chk);
  tcase_add_test (tc, tmutiltoms_chk);
  tcase_add_test (tc, tmutiltomsd_chk);
  tcase_add_test (tc, tmutiltodatehm_chk);
  tcase_add_test (tc, tmutil_strtoms);
  tcase_add_test (tc, tmutil_strtohm);
  suite_add_tcase (s, tc);
  return s;
}

