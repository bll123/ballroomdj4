#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <check.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bdjstring.h"
#include "check_bdj.h"
#include "lock.h"
#include "pathbld.h"
#include "sysvars.h"

#define FULL_LOCK_FN "tmp/test_lock.lck"
#define LOCK_FN "test_lock"

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
  rc = lockAcquirePid (LOCK_FN, pid, PATHBLD_MP_NONE);
  ck_assert_int_gt (rc, 0);
  rc = stat (FULL_LOCK_FN, &statbuf);
  ck_assert_int_eq (rc, 0);
  ck_assert_int_gt (statbuf.st_size, 0);
  fh = fopen (FULL_LOCK_FN, "r");
  rc = fscanf (fh, "%zd", &temp);
  fpid = (pid_t) temp;
  fclose (fh);
  ck_assert_int_eq (fpid, pid);
  rc = lockReleasePid (LOCK_FN, pid, PATHBLD_MP_NONE);
  ck_assert_int_eq (rc, 0);
  rc = stat (LOCK_FN, &statbuf);
  ck_assert_int_lt (rc, 0);
}
END_TEST

START_TEST(lock_already)
{
  int           rc;
  pid_t         pid;

  pid = getpid ();
  unlink (FULL_LOCK_FN);
  rc = lockAcquirePid (LOCK_FN, pid, PATHBLD_MP_NONE);
  ck_assert_int_gt (rc, 0);
  rc = lockAcquirePid (LOCK_FN, pid, PATHBLD_MP_NONE);
  ck_assert_int_lt (rc, 0);
  rc = lockReleasePid (LOCK_FN, pid, PATHBLD_MP_NONE);
  ck_assert_int_eq (rc, 0);
}
END_TEST

START_TEST(lock_other_dead)
{
  int           rc;
  pid_t         pid;

  pid = getpid ();
  unlink (FULL_LOCK_FN);
  rc = lockAcquirePid (LOCK_FN, 5, PATHBLD_MP_NONE);
  ck_assert_int_gt (rc, 0);
  rc = lockAcquirePid (LOCK_FN, pid, PATHBLD_MP_NONE);
  ck_assert_int_gt (rc, 0);
  rc = lockReleasePid (LOCK_FN, pid, PATHBLD_MP_NONE);
  ck_assert_int_eq (rc, 0);
}
END_TEST

START_TEST(lock_unlock_fail)
{
  int           rc;
  pid_t         pid;

  pid = getpid ();
  unlink (FULL_LOCK_FN);
  rc = lockAcquirePid (LOCK_FN, pid, PATHBLD_MP_NONE);
  ck_assert_int_gt (rc, 0);
  rc = lockReleasePid (LOCK_FN, 5, PATHBLD_MP_NONE);
  ck_assert_int_lt (rc, 0);
  rc = lockReleasePid (LOCK_FN, pid, PATHBLD_MP_NONE);
  ck_assert_int_eq (rc, 0);
}
END_TEST

START_TEST(lock_exists)
{
  int           rc;
  pid_t         pid;
  pid_t         fpid;
  size_t        temp;
  FILE          *fh;

  pid = getpid ();
  unlink (FULL_LOCK_FN);
  rc = lockAcquirePid (LOCK_FN, pid, PATHBLD_MP_NONE);
  ck_assert_int_gt (rc, 0);

  fh = fopen (FULL_LOCK_FN, "r");
  rc = fscanf (fh, "%zd", &temp);
  fpid = (pid_t) temp;
  fclose (fh);

//fprintf (stderr, "lock file exists; yes process\n");
  pid = lockExists (LOCK_FN, PATHBLD_MP_NONE);
  ck_assert_int_eq (fpid, pid);

  rc = lockReleasePid (LOCK_FN, pid, PATHBLD_MP_NONE);
  ck_assert_int_eq (rc, 0);
//fprintf (stderr, "lock file does not exist; no process\n");
  pid = lockExists (LOCK_FN, PATHBLD_MP_NONE);
  ck_assert_int_eq (pid, 0);

  fh = fopen (FULL_LOCK_FN, "w");
  temp = 94534;
  fprintf (fh, "%zd", temp);
  fclose (fh);
//fprintf (stderr, "lock file exists; no process\n");
  pid = lockExists (LOCK_FN, PATHBLD_MP_NONE);
  ck_assert_int_eq (pid, 0);
  unlink (FULL_LOCK_FN);
}
END_TEST

Suite *
lock_suite (void)
{
  Suite     *s;
  TCase     *tc;

  s = suite_create ("Lock Suite");
  tc = tcase_create ("Lock");
  tcase_add_test (tc, lock_acquire_release);
  tcase_add_test (tc, lock_already);
  tcase_add_test (tc, lock_other_dead);
  tcase_add_test (tc, lock_unlock_fail);
  tcase_add_test (tc, lock_exists);
  suite_add_tcase (s, tc);
  return s;
}
