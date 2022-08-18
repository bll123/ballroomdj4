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
#include "dance.h"
#include "dnctypes.h"
#include "ilist.h"
#include "log.h"
#include "slist.h"
#include "templateutil.h"

START_TEST(dance_alloc)
{
  dance_t   *dance = NULL;

  logMsg (LOG_DBG, LOG_IMPORTANT, "=== dance_alloc");

  templateFileCopy ("dancetypes.txt", "dancetypes.txt");
  templateFileCopy ("dances.txt", "dances.txt");

  bdjvarsdfloadInit ();
  dance = danceAlloc ();
  ck_assert_ptr_nonnull (dance);
  danceFree (dance);
  bdjvarsdfloadCleanup ();
}
END_TEST

START_TEST(dance_iterate)
{
  dance_t     *dance = NULL;
  char        *val = NULL;
  slistidx_t  iteridx;
  int         count;
  int         key;
  char        *ann;
  int         hbpm;
  int         lbpm;
  int         spd;
  slist_t     *tags;
  int         ts;
  int         type;

  logMsg (LOG_DBG, LOG_IMPORTANT, "=== dance_iterate");

  bdjvarsdfloadInit ();

  dance = danceAlloc ();
  danceStartIterator (dance, &iteridx);
  count = 0;
  while ((key = danceIterate (dance, &iteridx)) >= 0) {
    val = danceGetStr (dance, key, DANCE_DANCE);

    ann = danceGetStr (dance, key, DANCE_ANNOUNCE);
    hbpm = danceGetNum (dance, key, DANCE_HIGH_BPM);
    lbpm = danceGetNum (dance, key, DANCE_LOW_BPM);
    spd = danceGetNum (dance, key, DANCE_SPEED);
    ck_assert_int_ge (spd, 0);
    ck_assert_int_le (spd, 2);
    tags = danceGetList (dance, key, DANCE_TAGS);
    ck_assert_ptr_nonnull (tags);
    ck_assert_int_ge (slistGetCount (tags), 0);
    ck_assert_int_le (slistGetCount (tags), 2);
    ts = danceGetNum (dance, key, DANCE_TIMESIG);
    ck_assert_int_ge (ts, 0);
    ck_assert_int_le (ts, 3);
    type = danceGetNum (dance, key, DANCE_TYPE);
    /* assuming the default data file */
    ck_assert_int_ge (type, 0);
    ck_assert_int_le (type, 2);

    ++count;
  }
  ck_assert_int_eq (count, danceGetCount (dance));
  danceFree (dance);
  bdjvarsdfloadCleanup ();
}
END_TEST


START_TEST(dance_set)
{
  dance_t     *dance = NULL;
  char        *val = NULL;
  slistidx_t  iteridx;
  int         count;
  int         key;
  char        *ann;
  int         hbpm;
  int         lbpm;
  int         spd;
  slist_t     *tags;
  int         ts;
  int         type;
  char        *tval;
  char        *tann;
  int         thbpm;
  int         tlbpm;
  int         tspd;
  slist_t     *ttags;
  slistidx_t  ttiteridx;
  int         tts;
  int         ttype;

  logMsg (LOG_DBG, LOG_IMPORTANT, "=== dance_iterate");

  bdjvarsdfloadInit ();

  dance = danceAlloc ();
  danceStartIterator (dance, &iteridx);
  count = 0;
  while ((key = danceIterate (dance, &iteridx)) >= 0) {
    val = danceGetStr (dance, key, DANCE_DANCE);
    danceSetStr (dance, key, DANCE_DANCE, "abc123");
    tval = danceGetStr (dance, key, DANCE_DANCE);
    ck_assert_ptr_nonnull (tval);
    ck_assert_str_ne (val, tval);
    ck_assert_str_eq (tval, "abc123");
    danceSetStr (dance, key, DANCE_DANCE, val);

    ann = danceGetStr (dance, key, DANCE_ANNOUNCE);
    danceSetStr (dance, key, DANCE_ANNOUNCE, "abc123");
    tann = danceGetStr (dance, key, DANCE_ANNOUNCE);
    ck_assert_ptr_nonnull (tann);
    ck_assert_str_ne (ann, tann);
    ck_assert_str_eq (tann, "abc123");

    hbpm = danceGetNum (dance, key, DANCE_HIGH_BPM);
    danceSetNum (dance, key, DANCE_HIGH_BPM, 5);
    thbpm = danceGetNum (dance, key, DANCE_HIGH_BPM);
    ck_assert_int_eq (thbpm, 5);
    ck_assert_int_ne (hbpm, thbpm);

    lbpm = danceGetNum (dance, key, DANCE_LOW_BPM);
    danceSetNum (dance, key, DANCE_LOW_BPM, 5);
    tlbpm = danceGetNum (dance, key, DANCE_LOW_BPM);
    ck_assert_int_eq (tlbpm, 5);
    ck_assert_int_ne (lbpm, tlbpm);

    spd = danceGetNum (dance, key, DANCE_SPEED);
    danceSetNum (dance, key, DANCE_SPEED, DANCE_SPEED_SLOW);
    tspd = danceGetNum (dance, key, DANCE_SPEED);
    ck_assert_int_eq (tspd, DANCE_SPEED_SLOW);

    tags = danceGetList (dance, key, DANCE_TAGS);
    ttags = slistAlloc ("chk-dance-set-tags", LIST_ORDERED, NULL);
    slistSetNum (ttags, "tag", 0);
    danceSetList (dance, key, DANCE_TAGS, ttags);
    ttags = danceGetList (dance, key, DANCE_TAGS);
    ck_assert_int_eq (slistGetCount (ttags), 1);
    slistStartIterator (ttags, &ttiteridx);
    val = slistIterateKey (ttags, &ttiteridx);
    ck_assert_str_eq (val, "tag");

    ts = danceGetNum (dance, key, DANCE_TIMESIG);
    danceSetNum (dance, key, DANCE_TIMESIG, DANCE_TIMESIG_48);
    tts = danceGetNum (dance, key, DANCE_TIMESIG);
    ck_assert_int_eq (tts, DANCE_TIMESIG_48);

    type = danceGetNum (dance, key, DANCE_TYPE);
    danceSetNum (dance, key, DANCE_TYPE, 2);
    ttype = danceGetNum (dance, key, DANCE_TYPE);
    ck_assert_int_eq (ttype, 2);

    ++count;
  }
  ck_assert_int_eq (count, danceGetCount (dance));
  danceFree (dance);
  bdjvarsdfloadCleanup ();
}
END_TEST

START_TEST(dance_conv)
{
  dance_t     *dance = NULL;
  char        *val = NULL;
  slistidx_t  iteridx;
  datafileconv_t conv;
  int         count;
  int         key;
  int         type;

  logMsg (LOG_DBG, LOG_IMPORTANT, "=== dance_conv");

  bdjvarsdfloadInit ();

  dance = danceAlloc ();
  danceStartIterator (dance, &iteridx);
  count = 0;
  while ((key = danceIterate (dance, &iteridx)) >= 0) {
    val = danceGetStr (dance, key, DANCE_DANCE);

    conv.allocated = false;
    conv.valuetype = VALUE_STR;
    conv.str = val;
    danceConvDance (&conv);
    ck_assert_int_eq (conv.num, count);

    conv.allocated = false;
    conv.valuetype = VALUE_NUM;
    conv.num = count;
    danceConvDance (&conv);
    ck_assert_str_eq (conv.str, val);
    if (conv.allocated) {
      free (conv.str);
    }

    type = danceGetNum (dance, key, DANCE_TYPE);
    conv.allocated = false;
    conv.valuetype = VALUE_NUM;
    conv.num = type;
    dnctypesConv (&conv);
    dnctypesConv (&conv);
    ck_assert_int_eq (conv.num, type);

    ++count;
  }
  danceFree (dance);
  bdjvarsdfloadCleanup ();
}
END_TEST

START_TEST(dance_save)
{
  dance_t     *dance = NULL;
  char        *val = NULL;
  ilistidx_t  diteridx;
  ilistidx_t  iiteridx;
  int         key;
  ilist_t     *tlist;
  int         tkey;
  char        *ann;
  int         hbpm;
  int         lbpm;
  int         spd;
  slist_t     *tags;
  int         ts;
  int         type;
  char        *tval;
  char        *tann;
  int         thbpm;
  int         tlbpm;
  int         tspd;
  slist_t     *ttags;
  int         tts;
  int         ttype;
  slistidx_t  titeridx;
  slistidx_t  ttiteridx;

  logMsg (LOG_DBG, LOG_IMPORTANT, "=== dance_save");

  /* required for the dance conversion function */
  bdjvarsdfloadInit ();

  dance = danceAlloc ();

  danceStartIterator (dance, &diteridx);
  tlist = ilistAlloc ("chk-dance-a", LIST_ORDERED);
  while ((key = danceIterate (dance, &diteridx)) >= 0) {
    val = danceGetStr (dance, key, DANCE_DANCE);
    ann = danceGetStr (dance, key, DANCE_ANNOUNCE);
    hbpm = danceGetNum (dance, key, DANCE_HIGH_BPM);
    lbpm = danceGetNum (dance, key, DANCE_LOW_BPM);
    spd = danceGetNum (dance, key, DANCE_SPEED);
    tags = danceGetList (dance, key, DANCE_TAGS);
    ts = danceGetNum (dance, key, DANCE_TIMESIG);
    type = danceGetNum (dance, key, DANCE_TYPE);

    ilistSetStr (tlist, key, DANCE_DANCE, val);
    ilistSetStr (tlist, key, DANCE_ANNOUNCE, ann);
    ilistSetNum (tlist, key, DANCE_HIGH_BPM, hbpm);
    ilistSetNum (tlist, key, DANCE_LOW_BPM, lbpm);
    ilistSetNum (tlist, key, DANCE_SPEED, spd);
    ilistSetNum (tlist, key, DANCE_TIMESIG, ts);
    ilistSetNum (tlist, key, DANCE_TYPE, type);

    /* need a copy of the tags */
    ttags = slistAlloc ("chk-dance-ttags", LIST_ORDERED, free);
    slistStartIterator (tags, &titeridx);
    while ((val = slistIterateKey (tags, &titeridx)) != NULL) {
      slistSetNum (ttags, val, 0);
    }
    ilistSetList (tlist, key, DANCE_TAGS, ttags);
  }

  danceSave (dance, tlist);
  danceFree (dance);

  dance = danceAlloc ();

  ck_assert_int_eq (ilistGetCount (tlist), danceGetCount (dance));

  danceStartIterator (dance, &diteridx);
  ilistStartIterator (tlist, &iiteridx);
  while ((key = danceIterate (dance, &diteridx)) >= 0) {
    val = danceGetStr (dance, key, DANCE_DANCE);
    ann = danceGetStr (dance, key, DANCE_ANNOUNCE);
    hbpm = danceGetNum (dance, key, DANCE_HIGH_BPM);
    lbpm = danceGetNum (dance, key, DANCE_LOW_BPM);
    spd = danceGetNum (dance, key, DANCE_SPEED);
    tags = danceGetList (dance, key, DANCE_TAGS);
    ts = danceGetNum (dance, key, DANCE_TIMESIG);
    type = danceGetNum (dance, key, DANCE_TYPE);

    tkey = ilistIterateKey (tlist, &iiteridx);
    tval = ilistGetStr (tlist, key, DANCE_DANCE);
    tann = ilistGetStr (tlist, key, DANCE_ANNOUNCE);
    thbpm = ilistGetNum (tlist, key, DANCE_HIGH_BPM);
    tlbpm = ilistGetNum (tlist, key, DANCE_LOW_BPM);
    tspd = ilistGetNum (tlist, key, DANCE_SPEED);
    ttags = ilistGetList (tlist, key, DANCE_TAGS);
    tts = ilistGetNum (tlist, key, DANCE_TIMESIG);
    ttype = ilistGetNum (tlist, key, DANCE_TYPE);

    ck_assert_int_eq (key, tkey);
    ck_assert_str_eq (val, tval);
    ck_assert_str_eq (ann, tann);
    ck_assert_int_eq (hbpm, thbpm);
    ck_assert_int_eq (lbpm, tlbpm);
    ck_assert_int_eq (spd, tspd);
    ck_assert_int_eq (ts, tts);
    ck_assert_int_eq (type, ttype);

    ck_assert_int_eq (slistGetCount (tags), slistGetCount (ttags));

    slistSort (tags);
    slistStartIterator (tags, &titeridx);
    slistStartIterator (ttags, &ttiteridx);
    while ((val = slistIterateKey (tags, &titeridx)) != NULL) {
      tval = slistIterateKey (tags, &ttiteridx);
      ck_assert_str_eq (val, tval);
    }
  }

  ilistFree (tlist);
  danceFree (dance);
  bdjvarsdfloadCleanup ();
}
END_TEST

START_TEST(dance_delete)
{
  dance_t     *dance = NULL;
  ilistidx_t  key;
  ilistidx_t  dkey;
  ilistidx_t  diteridx;
  int         count;
  char        *val;
  char        *dval;

  logMsg (LOG_DBG, LOG_IMPORTANT, "=== dance_delete");

  bdjvarsdfloadInit ();
  dance = danceAlloc ();

  count = 0;
  danceStartIterator (dance, &diteridx);
  while ((key = danceIterate (dance, &diteridx)) >= 0) {
    if (count == 2) {
      dval = strdup (danceGetStr (dance, key, DANCE_DANCE));
      danceDelete (dance, key);
      dkey = key;
      break;
    }
    ++count;
  }

  danceStartIterator (dance, &diteridx);
  while ((key = danceIterate (dance, &diteridx)) >= 0) {
    val = danceGetStr (dance, key, DANCE_DANCE);
    ck_assert_int_ne (dkey, key);
    ck_assert_str_ne (val, dval);
  }
  free (dval);

  danceFree (dance);
  bdjvarsdfloadCleanup ();
}


START_TEST(dance_add)
{
  dance_t     *dance = NULL;
  ilistidx_t  key;
  ilistidx_t  diteridx;
  int         rc;
  char        *val;

  logMsg (LOG_DBG, LOG_IMPORTANT, "=== dance_add");

  bdjvarsdfloadInit ();
  dance = danceAlloc ();

  danceAdd (dance, "testdance");

  danceStartIterator (dance, &diteridx);
  rc = 0;
  while ((key = danceIterate (dance, &diteridx)) >= 0) {
    val = danceGetStr (dance, key, DANCE_DANCE);
    if (strcmp (val, "testdance") == 0) {
      rc = 1;
    }
  }
  ck_assert_int_eq (rc, 1);

  danceFree (dance);
  bdjvarsdfloadCleanup ();
}


Suite *
dance_suite (void)
{
  Suite     *s;
  TCase     *tc;

  s = suite_create ("dance");
  tc = tcase_create ("dance");
  tcase_add_test (tc, dance_alloc);
  tcase_add_test (tc, dance_iterate);
  tcase_add_test (tc, dance_set);
  tcase_add_test (tc, dance_conv);
  tcase_add_test (tc, dance_save);
  tcase_add_test (tc, dance_delete);
  tcase_add_test (tc, dance_add);
  suite_add_tcase (s, tc);
  return s;
}
