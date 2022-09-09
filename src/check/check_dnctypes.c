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

#include "bdjvarsdfload.h"
#include "check_bdj.h"
#include "datafile.h"
#include "dnctypes.h"
#include "log.h"
#include "slist.h"
#include "templateutil.h"

static void
setup (void)
{
  templateFileCopy ("dancetypes.txt", "dancetypes.txt");
}

START_TEST(dnctypes_alloc)
{
  dnctype_t *dt = NULL;

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- dnctypes_alloc");

  dt = dnctypesAlloc ();
  ck_assert_ptr_nonnull (dt);
  dnctypesFree (dt);
}
END_TEST

START_TEST(dnctypes_iterate)
{
  dnctype_t   *dt = NULL;
  char        *val = NULL;
  char        *prior = NULL;
  slistidx_t  iteridx;

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- dnctypes_iterate");

  dt = dnctypesAlloc ();
  dnctypesStartIterator (dt, &iteridx);
  while ((val = dnctypesIterate (dt, &iteridx)) != NULL) {
    if (prior != NULL) {
      ck_assert_str_gt (val, prior);
    }
    prior = val;
  }
  dnctypesFree (dt);
}
END_TEST

START_TEST(dnctypes_conv)
{
  dnctype_t   *dt = NULL;
  char        *val = NULL;
  slistidx_t  iteridx;
  datafileconv_t conv;
  int         count;

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- dnctypes_conv");

  bdjvarsdfloadInit ();

  dt = dnctypesAlloc ();
  dnctypesStartIterator (dt, &iteridx);
  count = 0;
  while ((val = dnctypesIterate (dt, &iteridx)) != NULL) {
    conv.allocated = false;
    conv.valuetype = VALUE_STR;
    conv.str = val;
    dnctypesConv (&conv);
    ck_assert_int_eq (conv.num, count);

    conv.allocated = false;
    conv.valuetype = VALUE_NUM;
    conv.num = count;
    dnctypesConv (&conv);
    ck_assert_str_eq (conv.str, val);
    if (conv.allocated) {
      free (conv.str);
    }

    ++count;
  }
  dnctypesFree (dt);

  bdjvarsdfloadCleanup ();
}
END_TEST

Suite *
dnctypes_suite (void)
{
  Suite     *s;
  TCase     *tc;

  s = suite_create ("dnctypes");
  tc = tcase_create ("dnctypes");
  tcase_set_tags (tc, "libbdj4");
  tcase_add_unchecked_fixture (tc, setup, NULL);
  tcase_add_test (tc, dnctypes_alloc);
  tcase_add_test (tc, dnctypes_iterate);
  tcase_add_test (tc, dnctypes_conv);
  suite_add_tcase (s, tc);
  return s;
}
