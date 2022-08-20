#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#pragma clang diagnostic push
#pragma gcc diagnostic push
#pragma clang diagnostic ignored "-Wformat-extra-args"
#pragma gcc diagnostic ignored "-Wformat-extra-args"

#include <check.h>

#include "lock.h"
#include "pathbld.h"
#include "rafile.h"
#include "check_bdj.h"

#define RAFN "tmp/test_rafile.dat"

START_TEST(rafile_create_new)
{
  rafile_t      *rafile;
  struct stat   statbuf;
  int           rc;

  unlink (RAFN);
  rafile = raOpen (RAFN, 10);
  ck_assert_ptr_nonnull (rafile);
  ck_assert_int_eq (raGetVersion (rafile), 10);
  ck_assert_int_eq (raGetSize (rafile), RAFILE_REC_SIZE);
  ck_assert_int_eq (raGetCount (rafile), 0);
  ck_assert_int_eq (raGetNextRRN (rafile), 1);
  raClose (rafile);
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  ck_assert_int_ge (statbuf.st_size, 59);
}
END_TEST

START_TEST(rafile_reopen)
{
  rafile_t      *rafile;

  rafile = raOpen (RAFN, 10);
  ck_assert_ptr_nonnull (rafile);
  ck_assert_int_eq (raGetVersion (rafile), 10);
  ck_assert_int_eq (raGetSize (rafile), RAFILE_REC_SIZE);
  ck_assert_int_eq (raGetCount (rafile), 0);
  ck_assert_int_eq (raGetNextRRN (rafile), 1);
  raClose (rafile);
}
END_TEST

START_TEST(rafile_write)
{
  rafile_t      *rafile;
  struct stat   statbuf;
  int           rc;
  off_t         lastsize;

  rafile = raOpen (RAFN, 10);
  ck_assert_ptr_nonnull (rafile);
  ck_assert_int_eq (raGetVersion (rafile), 10);
  ck_assert_int_eq (raGetSize (rafile), RAFILE_REC_SIZE);
  ck_assert_int_eq (raGetCount (rafile), 0);
  ck_assert_int_eq (raGetNextRRN (rafile), 1);
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  lastsize = statbuf.st_size;

  raWrite (rafile, RAFILE_NEW, "aaaa");
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  ck_assert_int_ge (statbuf.st_size, lastsize);
  lastsize = statbuf.st_size;
  ck_assert_int_eq (raGetCount (rafile), 1);
  ck_assert_int_eq (raGetNextRRN (rafile), 2);
  raWrite (rafile, RAFILE_NEW, "bbbb");
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  ck_assert_int_ge (statbuf.st_size, lastsize);
  lastsize = statbuf.st_size;
  ck_assert_int_eq (raGetCount (rafile), 2);
  ck_assert_int_eq (raGetNextRRN (rafile), 3);
  raWrite (rafile, RAFILE_NEW, "cccc");
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  ck_assert_int_ge (statbuf.st_size, lastsize);
  ck_assert_int_eq (raGetCount (rafile), 3);
  ck_assert_int_eq (raGetNextRRN (rafile), 4);

  raClose (rafile);
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  /* rawrite writes a single byte at the end so that the last record */
  /* has a size */
  ck_assert_int_eq (statbuf.st_size, RRN_TO_OFFSET(4L) + 1);
}
END_TEST

START_TEST(rafile_write_batch)
{
  rafile_t      *rafile;
  struct stat   statbuf;
  int           rc;
  off_t         lastsize;

  rafile = raOpen (RAFN, 10);
  ck_assert_ptr_nonnull (rafile);
  ck_assert_int_eq (raGetCount (rafile), 3);
  ck_assert_int_eq (raGetNextRRN (rafile), 4);
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  lastsize = statbuf.st_size;

  rc = lockExists (RAFILE_LOCK_FN, PATHBLD_MP_TMPDIR | LOCK_TEST_SKIP_SELF);
  ck_assert_int_eq (rc, getpid());
  raStartBatch (rafile);
  rc = lockExists (RAFILE_LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_eq (rc, 0);
  raWrite (rafile, RAFILE_NEW, "dddd");
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  ck_assert_int_ge (statbuf.st_size, lastsize);
  lastsize = statbuf.st_size;
  ck_assert_int_eq (raGetCount (rafile), 4);
  ck_assert_int_eq (raGetNextRRN (rafile), 5);
  raWrite (rafile, RAFILE_NEW, "eeee");
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  ck_assert_int_ge (statbuf.st_size, lastsize);
  lastsize = statbuf.st_size;
  ck_assert_int_eq (raGetCount (rafile), 5);
  ck_assert_int_eq (raGetNextRRN (rafile), 6);
  raWrite (rafile, RAFILE_NEW, "ffff");
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  ck_assert_int_ge (statbuf.st_size, lastsize);
  ck_assert_int_eq (raGetCount (rafile), 6);
  ck_assert_int_eq (raGetNextRRN (rafile), 7);
  rc = lockExists (RAFILE_LOCK_FN, PATHBLD_MP_TMPDIR | LOCK_TEST_SKIP_SELF);
  ck_assert_int_eq (rc, getpid());
  raEndBatch (rafile);
  rc = lockExists (RAFILE_LOCK_FN, PATHBLD_MP_TMPDIR);
  ck_assert_int_eq (rc, 0);

  raClose (rafile);
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  /* rawrite writes a single byte at the end so that the last record */
  /* has a size */
  ck_assert_int_eq (statbuf.st_size, RRN_TO_OFFSET(7L) + 1);
}
END_TEST

START_TEST(rafile_read)
{
  rafile_t      *rafile;
  char          data [RAFILE_REC_SIZE];
  int           rc;

  rafile = raOpen (RAFN, 10);
  ck_assert_ptr_nonnull (rafile);
  ck_assert_int_eq (raGetCount (rafile), 6);
  rc = raRead (rafile, 1, data);
  ck_assert_int_eq (rc, 1);
  ck_assert_str_eq (data, "aaaa");
  rc = raRead (rafile, 2, data);
  ck_assert_int_eq (rc, 1);
  ck_assert_str_eq (data, "bbbb");
  rc = raRead (rafile, 3, data);
  ck_assert_int_eq (rc, 1);
  ck_assert_str_eq (data, "cccc");
  ck_assert_int_eq (raGetNextRRN (rafile), 7);

  raClose (rafile);
}
END_TEST

START_TEST(rafile_rewrite)
{
  rafile_t      *rafile;
  struct stat   statbuf;
  ssize_t       rc;
  off_t         lastsize;

  rafile = raOpen (RAFN, 10);
  ck_assert_ptr_nonnull (rafile);
  ck_assert_int_eq (raGetVersion (rafile), 10);
  ck_assert_int_eq (raGetCount (rafile), 6);
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  lastsize = statbuf.st_size;

  raWrite (rafile, 3, "gggg");
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  ck_assert_int_eq (statbuf.st_size, lastsize);
  lastsize = statbuf.st_size;
  raWrite (rafile, 2, "hhhh");
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  ck_assert_int_eq (statbuf.st_size, lastsize);
  lastsize = statbuf.st_size;
  raWrite (rafile, 1, "iiii");
  rc = stat (RAFN, &statbuf);
  ck_assert_int_eq (rc, 0);
  ck_assert_int_eq (statbuf.st_size, lastsize);

  ck_assert_int_eq (raGetCount (rafile), 6);
  ck_assert_int_eq (raGetNextRRN (rafile), 7);
  raClose (rafile);
}
END_TEST

START_TEST(rafile_reread)
{
  rafile_t      *rafile;
  char          data [RAFILE_REC_SIZE];
  ssize_t       rc;

  rafile = raOpen (RAFN, 10);
  ck_assert_ptr_nonnull (rafile);
  ck_assert_int_eq (raGetCount (rafile), 6);
  rc = raRead (rafile, 1, data);
  ck_assert_int_eq (rc, 1);
  ck_assert_str_eq (data, "iiii");
  rc = raRead (rafile, 2, data);
  ck_assert_int_eq (rc, 1);
  ck_assert_str_eq (data, "hhhh");
  rc = raRead (rafile, 3, data);
  ck_assert_int_eq (rc, 1);
  ck_assert_str_eq (data, "gggg");
  ck_assert_int_eq (raGetNextRRN (rafile), 7);
  raClose (rafile);
}
END_TEST


START_TEST(rafile_bad_write_len)
{
  rafile_t      *rafile;
  ssize_t       rc;
  char          data [2 * RAFILE_REC_SIZE];

  rafile = raOpen (RAFN, 10);
  ck_assert_ptr_nonnull (rafile);

  memset (data, 0, sizeof (data));
  memset (data, 'a', sizeof (data)-4);
  rc = raWrite (rafile, 0, data);
  ck_assert_int_ne (rc, 0);
  raClose (rafile);
}
END_TEST

START_TEST(rafile_clear)
{
  rafile_t      *rafile;
  ssize_t       rc;
  char          data [2 * RAFILE_REC_SIZE];

  rafile = raOpen (RAFN, 10);
  ck_assert_ptr_nonnull (rafile);

  rc = raClear (rafile, 2);
  ck_assert_int_eq (rc, 0);

  rc = raRead (rafile, 1, data);
  ck_assert_int_eq (rc, 1);
  ck_assert_str_eq (data, "iiii");
  rc = raRead (rafile, 2, data);
  ck_assert_int_eq (rc, 1);
  ck_assert_str_eq (data, "");
  rc = raRead (rafile, 3, data);
  ck_assert_int_eq (rc, 1);
  ck_assert_str_eq (data, "gggg");

  raClose (rafile);
}
END_TEST

START_TEST(rafile_bad_read)
{
  rafile_t      *rafile;
  ssize_t        rc;
  char          data [RAFILE_REC_SIZE];

  rafile = raOpen (RAFN, 10);
  ck_assert_ptr_nonnull (rafile);

  rc = raRead (rafile, 0, data);
  ck_assert_int_eq (rc, 0);
  rc = raRead (rafile, 9, data);
  ck_assert_int_eq (rc, 0);

  raClose (rafile);
}
END_TEST

START_TEST(rafile_bad_clear)
{
  rafile_t      *rafile;
  ssize_t       rc;

  rafile = raOpen (RAFN, 10);
  ck_assert_ptr_nonnull (rafile);

  rc = raClear (rafile, 0);
  ck_assert_int_ne (rc, 0);
  rc = raClear (rafile, 9);
  ck_assert_int_ne (rc, 0);

  raClose (rafile);
}
END_TEST

START_TEST(rafile_cleanup)
{
  unlink (RAFN);
}
END_TEST

Suite *
rafile_suite (void)
{
  Suite     *s;
  TCase     *tc;

  s = suite_create ("rafile");
  tc = tcase_create ("rafile");
  tcase_add_test (tc, rafile_create_new);
  tcase_add_test (tc, rafile_reopen);
  tcase_add_test (tc, rafile_write);
  tcase_add_test (tc, rafile_write_batch);
  tcase_add_test (tc, rafile_read);
  tcase_add_test (tc, rafile_rewrite);
  tcase_add_test (tc, rafile_reread);
  tcase_add_test (tc, rafile_bad_write_len);
  tcase_add_test (tc, rafile_clear);
  tcase_add_test (tc, rafile_bad_read);
  tcase_add_test (tc, rafile_bad_clear);
  tcase_add_test (tc, rafile_cleanup);
  suite_add_tcase (s, tc);
  return s;
}
