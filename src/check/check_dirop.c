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

#include "check_bdj.h"
#include "dirlist.h"
#include "dirop.h"
#include "filemanip.h"
#include "fileop.h"
#include "log.h"
#include "slist.h"
#include "sysvars.h"

START_TEST(dirop_mkdir_isdir_a)
{
  int       rc;
  char *fn = "tmp/def";

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- dirop_mkdir_isdir_a");

  unlink (fn);
  rmdir (fn);
  fn = "tmp/abc/def";
  rc = diropMakeDir (fn);
  ck_assert_int_eq (rc, 0);
  fn = "tmp/abc/xyz";
  rc = diropMakeDir (fn);
  ck_assert_int_eq (rc, 0);

  rc = fileopIsDirectory ("tmp/def");
  ck_assert_int_eq (rc, 0);

  rc = fileopFileExists ("tmp/def");
  ck_assert_int_eq (rc, 0);

  rc = fileopIsDirectory ("tmp/abc");
  ck_assert_int_eq (rc, 1);

  rc = fileopFileExists ("tmp/abc");
  ck_assert_int_eq (rc, 0);

  rc = fileopIsDirectory (fn);
  ck_assert_int_eq (rc, 1);
  /* exists will return false on a directory */
  rc = fileopFileExists ("fn");
  ck_assert_int_eq (rc, 0);

  rc = fileopIsDirectory ("tmp/abc/xyz");
  ck_assert_int_eq (rc, 1);

  rmdir (fn);
  rmdir ("tmp/abc/xyz");
  rmdir ("tmp/abc");
}
END_TEST

START_TEST(dirop_del_dir_a)
{
  FILE      *fh;
  int       rc;
  char *dfn = "tmp/abc";
  char *ofn = "tmp/abc/def";
  char *nfn = "tmp/abc/def/xyz.txt";

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- dirop_del_dir_a");

  /* note that this fails on windows running under msys2 */
  rc = diropMakeDir (dfn);
  ck_assert_int_eq (rc, 0);
  rc = diropMakeDir (ofn);
  ck_assert_int_eq (rc, 0);
  fh = fopen (nfn, "w");
  ck_assert_ptr_nonnull (fh);
  fclose (fh);
  rc = fileopIsDirectory (dfn);
  ck_assert_int_eq (rc, 1);
  rc = fileopIsDirectory (ofn);
  ck_assert_int_eq (rc, 1);
  rc = fileopFileExists (nfn);
  ck_assert_int_eq (rc, 1);

  diropDeleteDir (dfn);
  rc = fileopFileExists (nfn);
  ck_assert_int_eq (rc, 0);
  rc = fileopIsDirectory (ofn);
  ck_assert_int_eq (rc, 0);
  rc = fileopIsDirectory (dfn);
  ck_assert_int_eq (rc, 0);
}
END_TEST

/* update the fnlist in fileop/filemanip/dirop/dirlist also */
static char *fnlist [] = {
  "tmp/abc-def",
  "tmp/????????",
  "tmp/I Am the Best_?????? ?????? ??? ??????",
  "tmp/?????????",
  "tmp/???????????????",
  "tmp/Ne_??????????????_??????????",
};
enum {
  fnlistsz = sizeof (fnlist) / sizeof (char *),
};

START_TEST(dirop_mk_is_del_u)
{
  int     rc;

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- dirop_mk_is_del_u");

  for (int i = 0; i < fnlistsz; ++i) {
    char *fn = fnlist [i];
    fileopDelete (fn);
    diropDeleteDir (fn);

    rc = diropMakeDir (fn);
    ck_assert_int_eq (rc, 0);
    rc = fileopFileExists (fn);
    ck_assert_int_eq (rc, 0);
    rc = fileopIsDirectory (fn);
    ck_assert_int_eq (rc, 1);
    diropDeleteDir (fn);
    rc = fileopIsDirectory (fn);
    ck_assert_int_eq (rc, 0);
  }
}
END_TEST

Suite *
dirop_suite (void)
{
  Suite     *s;
  TCase     *tc;

  s = suite_create ("dirop");
  tc = tcase_create ("dirop");
  tcase_set_tags (tc, "libcommon");
  tcase_add_test (tc, dirop_mkdir_isdir_a);
  tcase_add_test (tc, dirop_del_dir_a);
  tcase_add_test (tc, dirop_mk_is_del_u);
  suite_add_tcase (s, tc);
  return s;
}

