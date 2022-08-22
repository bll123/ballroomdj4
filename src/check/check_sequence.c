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

#include "bdjopt.h"
#include "bdjvarsdfload.h"
#include "check_bdj.h"
#include "filedata.h"
#include "filemanip.h"
#include "fileop.h"
#include "nlist.h"
#include "slist.h"
#include "sequence.h"
#include "templateutil.h"

#define SEQFN "test-sequence"

static void
setup (void)
{
  templateFileCopy ("dancetypes.txt", "dancetypes.txt");
  templateFileCopy ("dances.txt", "dances.txt");
  templateFileCopy ("genres.txt", "genres.txt");
  templateFileCopy ("levels.txt", "levels.txt");
  templateFileCopy ("ratings.txt", "ratings.txt");
  filemanipCopy ("test-templates/musicdb.dat", "data/musicdb.dat");
  filemanipCopy ("test-templates/test-sequence.sequence", "data/test-sequence.sequence");
  filemanipCopy ("test-templates/test-sequence.pl", "data/test-sequence.pl");
  filemanipCopy ("test-templates/test-sequence.pldances", "data/test-sequence.pldances");
}

START_TEST(sequence_create)
{
  sequence_t    *seq;
  slist_t       *tlist;

  bdjoptInit ();
  bdjoptSetStr (OPT_M_DIR_MUSIC, "test-music");
  bdjvarsdfloadInit ();

  seq = sequenceCreate (SEQFN);
  ck_assert_ptr_nonnull (seq);
  tlist = sequenceGetDanceList (seq);
  ck_assert_int_eq (slistGetCount (tlist), 0);
  sequenceFree (seq);

  bdjvarsdfloadCleanup ();
  bdjoptCleanup ();
}
END_TEST

START_TEST(sequence_alloc)
{
  sequence_t    *seq;
  slist_t       *tlist;

  bdjoptInit ();
  bdjoptSetStr (OPT_M_DIR_MUSIC, "test-music");
  bdjvarsdfloadInit ();

  seq = sequenceAlloc (SEQFN);
  ck_assert_ptr_nonnull (seq);
  tlist = sequenceGetDanceList (seq);
  ck_assert_int_eq (slistGetCount (tlist), 5);
  sequenceFree (seq);

  bdjvarsdfloadCleanup ();
  bdjoptCleanup ();
}
END_TEST

START_TEST(sequence_iterate)
{
  sequence_t    *seq;
  nlistidx_t    iteridx;
  nlistidx_t    fkey;
  nlistidx_t    key;
  int           count;

  bdjoptInit ();
  bdjoptSetStr (OPT_M_DIR_MUSIC, "test-music");
  bdjvarsdfloadInit ();

  seq = sequenceAlloc (SEQFN);
  ck_assert_ptr_nonnull (seq);
  sequenceStartIterator (seq, &iteridx);
  fkey = sequenceIterate (seq, &iteridx);
  ck_assert_int_ge (fkey, 0);
  count = 1;
  while ((key = sequenceIterate (seq, &iteridx)) != fkey) {
    ck_assert_int_ge (key, 0);
    ++count;
  }
  ck_assert_int_eq (count, 5);
  sequenceFree (seq);

  bdjvarsdfloadCleanup ();
  bdjoptCleanup ();
}
END_TEST

START_TEST(sequence_save)
{
  sequence_t    *seq;
  sequence_t    *seqb;
  slist_t       *tlist;
  slist_t       *tlistb;
  nlistidx_t    iteridx;
  nlistidx_t    iteridxb;
  nlistidx_t    siteridx;
  nlistidx_t    siteridxb;
  nlistidx_t    fkey;
  nlistidx_t    key;
  nlistidx_t    tkey;
  slist_t       *tslist;
  char          *stra;
  char          *strb;

  bdjoptInit ();
  bdjoptSetStr (OPT_M_DIR_MUSIC, "test-music");
  bdjvarsdfloadInit ();

  seq = sequenceAlloc (SEQFN);
  ck_assert_ptr_nonnull (seq);
  tlist = sequenceGetDanceList (seq);
  ck_assert_int_eq (slistGetCount (tlist), 5);
  tslist = slistAlloc ("chk-seq-save", LIST_UNORDERED, free);
  slistSetStr (tslist, "Waltz", 0);
  slistSetStr (tslist, "Tango", 0);
  slistSetStr (tslist, "Viennese Waltz", 0);
  slistSetStr (tslist, "Foxtrot", 0);
  slistSetStr (tslist, "Quickstep", 0);
  sequenceSave (seq, tslist);
  slistFree (tslist);

  seqb = sequenceAlloc (SEQFN);

  tlist = sequenceGetDanceList (seq);
  slistStartIterator (tlist, &siteridx);
  ck_assert_int_eq (slistGetCount (tlist), 5);
  tlistb = sequenceGetDanceList (seqb);
  slistStartIterator (tlistb, &siteridxb);
  ck_assert_int_eq (slistGetCount (tlistb), 5);

  sequenceStartIterator (seq, &iteridx);
  sequenceStartIterator (seqb, &iteridxb);
  fkey = sequenceIterate (seq, &iteridx);
  tkey = sequenceIterate (seqb, &iteridxb);
  ck_assert_int_eq (fkey, tkey);

  while ((key = sequenceIterate (seq, &iteridx)) != fkey) {
    tkey = sequenceIterate (seqb, &iteridxb);
    ck_assert_int_eq (key, tkey);
  }

  while ((stra = slistIterateKey (tlist, &siteridx)) != NULL) {
    strb = slistIterateKey (tlistb, &siteridxb);
    ck_assert_str_eq (stra, strb);
  }

  sequenceFree (seq);
  sequenceFree (seqb);

  bdjvarsdfloadCleanup ();
  bdjoptCleanup ();
}
END_TEST

Suite *
sequence_suite (void)
{
  Suite     *s;
  TCase     *tc;

  s = suite_create ("sequence");
  tc = tcase_create ("sequence");
  tcase_add_unchecked_fixture (tc, setup, NULL);
  tcase_add_test (tc, sequence_create);
  tcase_add_test (tc, sequence_alloc);
  tcase_add_test (tc, sequence_iterate);
  tcase_add_test (tc, sequence_save);
  suite_add_tcase (s, tc);
  return s;
}
