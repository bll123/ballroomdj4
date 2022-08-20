#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma gcc diagnostic push
#pragma clang diagnostic ignored "-Wformat-extra-args"
#pragma gcc diagnostic ignored "-Wformat-extra-args"

#include <check.h>

#include "bdjmsg.h"
#include "bdjstring.h"
#include "check_bdj.h"
#include "lock.h"
#include "pathbld.h"
#include "sysvars.h"

#define FULL_LOCK_FN "tmp/test_lock.lck"
#define LOCK_FN "test_lock"

START_TEST(lock_name)
{
  ck_assert_str_eq (lockName (ROUTE_PLAYERUI), "playerui");
}

START_TEST(lock_acquire_release)
{
  int           rc;
  struct stat   statbuf;
  FILE          *fh;
  pid_t         pid;
  pid_t         fpid;
  size_t        temp;

  pid = getpid ();
  unlink (FULL_LOCK_FN);
  rc = lockAcquire (LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_gt (rc, 0);
  rc = stat (FULL_LOCK_FN, &statbuf);
  ck_assert_int_eq (rc, 0);
  ck_assert_int_gt (statbuf.st_size, 0);
  fh = fopen (FULL_LOCK_FN, "r");
  rc = fscanf (fh, "%zd", &temp);
  fpid = (pid_t) temp;
  fclose (fh);
  ck_assert_int_eq (fpid, pid);
  rc = lockRelease (LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_eq (rc, 0);
  rc = stat (LOCK_FN, &statbuf);
  ck_assert_int_lt (rc, 0);
  unlink (FULL_LOCK_FN);
}
END_TEST

START_TEST(lock_already)
{
  int           rc;
  pid_t         pid;

  pid = getpid ();
  unlink (FULL_LOCK_FN);
  rc = lockAcquire (LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_gt (rc, 0);
  rc = lockAcquire (LOCK_FN, PATHBLD_MP_TMPDIR | LOCK_TEST_SKIP_SELF);
  ck_assert_int_lt (rc, 0);
  rc = lockRelease (LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_eq (rc, 0);
  unlink (FULL_LOCK_FN);
}
END_TEST

START_TEST(lock_other_dead)
{
  int           rc;
  pid_t         pid;

  pid = getpid ();
  unlink (FULL_LOCK_FN);
  rc = lockAcquire (LOCK_FN, PATHBLD_MP_TMPDIR | LOCK_TEST_OTHER_PID);
  ck_assert_int_gt (rc, 0);
  rc = lockAcquire (LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_gt (rc, 0);
  rc = lockRelease (LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_eq (rc, 0);
  unlink (FULL_LOCK_FN);
}
END_TEST

START_TEST(lock_unlock_fail)
{
  int           rc;
  pid_t         pid;

  pid = getpid ();
  unlink (FULL_LOCK_FN);
  rc = lockAcquire (LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_gt (rc, 0);
  rc = lockRelease (LOCK_FN, PATHBLD_MP_TMPDIR | LOCK_TEST_OTHER_PID);
  ck_assert_int_lt (rc, 0);
  rc = lockRelease (LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_eq (rc, 0);
  unlink (FULL_LOCK_FN);
}
END_TEST

START_TEST(lock_exists)
{
  int           rc;
  pid_t         pid;
  pid_t         tpid;
  pid_t         fpid;
  size_t        temp;
  FILE          *fh;

  pid = getpid ();
  unlink (FULL_LOCK_FN);
  rc = lockAcquire (LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_gt (rc, 0);

  fh = fopen (FULL_LOCK_FN, "r");
  rc = fscanf (fh, "%zd", &temp);
  fpid = (pid_t) temp;
  fclose (fh);
  ck_assert_int_eq (fpid, pid);

  /* lock file exists, same process */
  tpid = lockExists (LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_eq (tpid, 0);

  rc = lockRelease (LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_eq (rc, 0);
  /* lock file does not exist */
  tpid = lockExists (LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_eq (tpid, 0);

  fh = fopen (FULL_LOCK_FN, "w");
  temp = 94534;
  fprintf (fh, "%zd", temp);
  fclose (fh);
  /* lock file exists, no associated process */
  tpid = lockExists (LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_eq (tpid, 0);
  unlink (FULL_LOCK_FN);
}
END_TEST

Suite *
lock_suite (void)
{
  Suite     *s;
  TCase     *tc;

  s = suite_create ("lock");
  tc = tcase_create ("lock-base");
  tcase_add_test (tc, lock_name);
  tcase_add_test (tc, lock_acquire_release);
  suite_add_tcase (s, tc);
  tc = tcase_create ("lock-already");
  tcase_set_tags (tc, "slow");
  tcase_add_test (tc, lock_already);
  suite_add_tcase (s, tc);
  tc = tcase_create ("lock-more");
  tcase_add_test (tc, lock_other_dead);
  tcase_add_test (tc, lock_unlock_fail);
  tcase_add_test (tc, lock_exists);
  suite_add_tcase (s, tc);
  return s;
}

