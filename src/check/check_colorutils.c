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
#include "colorutils.h"

START_TEST(colorutils_chk)
{
  char    c [200];
  char    cb [200];

  createRandomColor (c, sizeof (c));
  ck_assert_int_eq (strlen (c), 7);
  createRandomColor (cb, sizeof (cb));
  ck_assert_int_eq (strlen (cb), 7);
  ck_assert_str_ne (c, cb);
}
END_TEST


Suite *
colorutils_suite (void)
{
  Suite     *s;
  TCase     *tc;

  s = suite_create ("colorutils");
  tc = tcase_create ("colorutils");
  tcase_set_tags (tc, "libcommon");
  tcase_add_test (tc, colorutils_chk);
  suite_add_tcase (s, tc);
  return s;
}

