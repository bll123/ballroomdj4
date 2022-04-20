#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "bdj4.h"
#include "bdjstring.h"
#include "bdjvarsdf.h"
#include "dance.h"
#include "datafile.h"
#include "dnctypes.h"
#include "fileop.h"
#include "ilist.h"
#include "log.h"
#include "pathbld.h"
#include "slist.h"

static void danceConvSpeed (datafileconv_t *conv);
static void danceConvTimeSig (datafileconv_t *conv);

  /* must be sorted in ascii order */
static datafilekey_t dancedfkeys [DANCE_KEY_MAX] = {
  { "ANNOUNCE",   DANCE_ANNOUNCE, VALUE_STR, NULL, -1 },
  { "DANCE",      DANCE_DANCE,    VALUE_STR, NULL, -1 },
  { "HIGHBPM",    DANCE_HIGH_BPM, VALUE_NUM, NULL, -1 },
  { "LOWBPM",     DANCE_LOW_BPM,  VALUE_NUM, NULL, -1 },
  { "SPEED",      DANCE_SPEED,    VALUE_NUM, danceConvSpeed, -1 },
  { "TAGS",       DANCE_TAGS,     VALUE_LIST, convTextList, -1 },
  { "TIMESIG",    DANCE_TIMESIG,  VALUE_NUM, danceConvTimeSig, -1 },
  { "TYPE",       DANCE_TYPE,     VALUE_NUM, dnctypesConv, -1 },
};

static datafilekey_t dancespeeddfkeys [DANCE_SPEED_MAX] = {
  { "fast",       DANCE_SPEED_FAST,   VALUE_NUM, NULL, -1 },
  { "normal",     DANCE_SPEED_NORMAL, VALUE_NUM, NULL, -1 },
  { "slow",       DANCE_SPEED_SLOW,   VALUE_NUM, NULL, -1 },
};

static datafilekey_t dancetimesigdfkeys [DANCE_TIMESIG_MAX] = {
  { "2/4",       DANCE_TIMESIG_24,   VALUE_NUM, NULL, -1 },
  { "3/4",       DANCE_TIMESIG_34,   VALUE_NUM, NULL, -1 },
  { "4/4",       DANCE_TIMESIG_44,   VALUE_NUM, NULL, -1 },
  { "4/8",       DANCE_TIMESIG_48,   VALUE_NUM, NULL, -1 },
};

dance_t *
danceAlloc (void)
{
  dance_t   *dance;
  char      fname [MAXPATHLEN];

  pathbldMakePath (fname, sizeof (fname), "", "dances", ".txt", PATHBLD_MP_NONE);
  if (! fileopFileExists (fname)) {
    logMsg (LOG_DBG, LOG_IMPORTANT, "dance: missing: %s", fname);
    return NULL;
  }

  dance = malloc (sizeof (dance_t));
  assert (dance != NULL);

  dance->path = strdup (fname);
  dance->danceList = NULL;
  dance->df = datafileAllocParse ("dance", DFTYPE_INDIRECT, fname,
      dancedfkeys, DANCE_KEY_MAX, DANCE_DANCE);
  dance->dances = datafileGetList (dance->df);
  return dance;
}

void
danceFree (dance_t *dance)
{
  if (dance != NULL) {
    if (dance->path != NULL) {
      free (dance->path);
    }
    if (dance->df != NULL) {
      datafileFree (dance->df);
    }
    if (dance->danceList != NULL) {
      slistFree (dance->danceList);
    }
    free (dance);
  }
}

void
danceStartIterator (dance_t *dances, ilistidx_t *iteridx)
{
  if (dances == NULL || dances->dances == NULL) {
    return;
  }

  ilistStartIterator (dances->dances, iteridx);
}

ilistidx_t
danceIterate (dance_t *dances, ilistidx_t *iteridx)
{
  ilistidx_t     ikey;

  if (dances == NULL || dances->dances == NULL) {
    return LIST_LOC_INVALID;
  }

  ikey = ilistIterateKey (dances->dances, iteridx);
  return ikey;
}

ssize_t
danceGetCount (dance_t *dance)
{
  return ilistGetCount (dance->dances);
}

char *
danceGetStr (dance_t *dance, ilistidx_t dkey, ilistidx_t idx)
{
  return ilistGetStr (dance->dances, dkey, idx);
}

slist_t *
danceGetList (dance_t *dance, ilistidx_t dkey, ilistidx_t idx)
{
  return ilistGetList (dance->dances, dkey, idx);
}

ssize_t
danceGetNum (dance_t *dance, ilistidx_t dkey, ilistidx_t idx)
{
  return ilistGetNum (dance->dances, dkey, idx);
}

void
danceSetStr (dance_t *dances, ilistidx_t dkey, ilistidx_t idx, const char *str)
{
  ilistSetStr (dances->dances, dkey, idx, str);
}

void
danceSetNum (dance_t *dances, ilistidx_t dkey, ilistidx_t idx, ssize_t value)
{
  ilistSetNum (dances->dances, dkey, idx, value);
}

void
danceSetList (dance_t *dances, ilistidx_t dkey, ilistidx_t idx, slist_t *list)
{
  ilistSetList (dances->dances, dkey, idx, list);
}

slist_t *
danceGetDanceList (dance_t *dance)
{
  slist_t     *dl;
  ilistidx_t  key;
  char        *nm;
  ilistidx_t  iteridx;

  if (dance->danceList != NULL) {
    return dance->danceList;
  }

  dl = slistAlloc ("dancelist", LIST_UNORDERED, NULL);
  slistSetSize (dl, ilistGetCount (dance->dances));
  ilistStartIterator (dance->dances, &iteridx);
  while ((key = ilistIterateKey (dance->dances, &iteridx)) >= 0) {
    nm = ilistGetStr (dance->dances, key, DANCE_DANCE);
    slistSetNum (dl, nm, key);
  }
  slistSort (dl);

  dance->danceList = dl;
  return dl;
}

void
danceConvDance (datafileconv_t *conv)
{
  dance_t   *dance;
  slist_t   *lookup;
  ssize_t   num;

  dance = bdjvarsdfGet (BDJVDF_DANCES);

  conv->allocated = false;
  if (conv->valuetype == VALUE_STR) {
    conv->valuetype = VALUE_NUM;
    num = -1;
    lookup = datafileGetLookup (dance->df);
    if (lookup != NULL) {
      num = slistGetNum (lookup, conv->u.str);
    }
    conv->u.num = num;
  } else if (conv->valuetype == VALUE_NUM) {
    conv->valuetype = VALUE_STR;
    num = conv->u.num;
    conv->u.str = ilistGetStr (dance->dances, num, DANCE_DANCE);
  }
}

void
danceSave (dance_t *dances, ilist_t *list)
{
  datafileSaveIndirect ("dance", dances->path, dancedfkeys,
      DANCE_KEY_MAX, list);
}

void
danceDelete (dance_t *dances, ilistidx_t dkey)
{
  ilistDelete (dances->dances, dkey);
}

ilistidx_t
danceAdd (dance_t *dances, char *name)
{
  ilistidx_t    count;

  count = ilistGetCount (dances->dances);
  ilistSetStr (dances->dances, count, DANCE_DANCE, name);
  ilistSetNum (dances->dances, count, DANCE_SPEED, DANCE_SPEED_NORMAL);
  ilistSetNum (dances->dances, count, DANCE_TYPE, 0);
  ilistSetNum (dances->dances, count, DANCE_TIMESIG, DANCE_TIMESIG_44);
  ilistSetNum (dances->dances, count, DANCE_HIGH_BPM, 0);
  ilistSetNum (dances->dances, count, DANCE_LOW_BPM, 0);
  ilistSetStr (dances->dances, count, DANCE_ANNOUNCE, "");
  ilistSetList (dances->dances, count, DANCE_TAGS, NULL);
  return count;
}

/* internal routines */

static void
danceConvSpeed (datafileconv_t *conv)
{
  nlistidx_t       idx;

  conv->allocated = false;
  if (conv->valuetype == VALUE_STR) {
    conv->valuetype = VALUE_NUM;
    idx = dfkeyBinarySearch (dancespeeddfkeys, DANCE_SPEED_MAX, conv->u.str);
    conv->u.num = dancespeeddfkeys [idx].itemkey;
  } else if (conv->valuetype == VALUE_NUM) {
    conv->valuetype = VALUE_STR;
    conv->u.str = dancespeeddfkeys [conv->u.num].name;
  }
}

static void
danceConvTimeSig (datafileconv_t *conv)
{
  nlistidx_t       idx;

  conv->allocated = false;
  if (conv->valuetype == VALUE_STR) {
    conv->valuetype = VALUE_NUM;
    idx = dfkeyBinarySearch (dancetimesigdfkeys, DANCE_TIMESIG_MAX, conv->u.str);
    conv->u.num = dancetimesigdfkeys [idx].itemkey;
  } else if (conv->valuetype == VALUE_NUM) {
    conv->valuetype = VALUE_STR;
    conv->u.str = dancetimesigdfkeys [conv->u.num].name;
  }
}
