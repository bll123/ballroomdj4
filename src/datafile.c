#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "datafile.h"
#include "tmutil.h"
#include "list.h"
#include "fileutil.h"
#include "bdjstring.h"

typedef enum {
  PARSE_SIMPLE,
  PARSE_KEYVALUE
} parsetype_t;

static size_t parse (parseinfo_t *pi, char *data, parsetype_t parsetype);
static void   datafileFreeInternal (datafile_t *df);
static long   dfkeyBinarySearch (const datafilekey_t *dfkeys,
                  size_t count, char *key);

/* parsing routines */

parseinfo_t *
parseInit (void)
{
  parseinfo_t   *pi;

  pi = malloc (sizeof (parseinfo_t));
  assert (pi != NULL);
  pi->strdata = NULL;
  pi->allocCount = 0;
  pi->count = 0;
  return pi;
}

void
parseFree (parseinfo_t *pi)
{
  if (pi != NULL) {
    if (pi->strdata != NULL) {
      free (pi->strdata);
    }
    free (pi);
  }
}

char **
parseGetData (parseinfo_t *pi)
{
  return pi->strdata;
}

size_t
parseSimple (parseinfo_t *pi, char *data)
{
  return parse (pi, data, PARSE_SIMPLE);
}

size_t
parseKeyValue (parseinfo_t *pi, char *data)
{
  return parse (pi, data, PARSE_KEYVALUE);
}

long
parseConvBoolean (const char *data)
{
  long      val = 0;

  if (strcmp (data, "on") == 0 ||
      strcmp (data, "yes") == 0 ||
      strcmp (data, "1") == 0) {
    val = 1;
  }
  return val;
}

/* datafile loading routines */

datafile_t *
datafileAlloc (datafilekey_t *dfkeys, size_t dfkeycount, char *fname,
    datafiletype_t dftype)
{
  datafile_t      *df;

  df = malloc (sizeof (datafile_t));
  assert (df != NULL);
  df->fname = NULL;
  df->data = NULL;
  datafileLoad (dfkeys, dfkeycount, df, fname, dftype);
  return df;
}

void
datafileFree (void *tdf)
{
  datafile_t    *df = (datafile_t *) tdf;

  if (df != NULL) {
    datafileFreeInternal (df);
    free (df);
  }
}

void
datafileLoad (datafilekey_t *dfkeys, size_t dfkeycount,
    datafile_t *df, char *fname, datafiletype_t dftype)
{
  char          *data = NULL;
  char          **strdata = NULL;
  parseinfo_t   *pi = NULL;
  long          key = -1L;
  size_t        dataCount;
  list_t        *itemList = NULL;
  valuetype_t   vt;
  size_t        inc = 2;
  char          *keyString = NULL;
  int           first = 1;
  long          ikey;
  long          lval;
  char          *ikeystr = NULL;
  char          *tkeystr;
  char          *tvalstr;

  /* may be re-using the song list */
  datafileFreeInternal (df);
  df->fname = strdup (fname);
  df->dftype = dftype;

  data = fileReadAll (df->fname);
  pi = parseInit ();
  if (dftype == DFTYPE_LIST) {
    dataCount = parseSimple (pi, data);
  } else {
    dataCount = parseKeyValue (pi, data);
  }
  strdata = parseGetData (pi);

  switch (dftype) {
    case DFTYPE_LIST: {
      inc = 1;
      df->data = listAlloc (sizeof (char *), NULL, free);
      listSetSize (df->data, dataCount);
      break;
    }
    case DFTYPE_KEY_STRING: {
      inc = 2;
      df->data = slistAlloc (LIST_UNORDERED, istringCompare, free, listFree);
      break;
    }
    case DFTYPE_KEY_LONG: {
      inc = 2;
      df->data = llistAlloc (LIST_UNORDERED, listFree);
      break;
    }
    case DFTYPE_KEY_VAL: {
      inc = 2;
      df->data = llistAlloc (LIST_ORDERED, free);
      break;
    }
    default: {
      break;
    }
  }

  for (size_t i = 0; i < dataCount; i += inc) {
    tkeystr = strdata [i];
    tvalstr = strdata [i + 1];

    if (inc == 2 && strcmp (strdata [i], "version") == 0) {
      df->version = atol (tvalstr);
      continue;
    }
    if (strcmp (strdata [i], "count") == 0) {
      if (dftype == DFTYPE_KEY_LONG) {
        llistSetSize (df->data, (size_t) atol (tvalstr));
      }
      if (dftype == DFTYPE_KEY_STRING) {
        slistSetSize (df->data, (size_t) atol (tvalstr));
      }
      continue;
    }

    if (dftype == DFTYPE_KEY_LONG &&
        strcmp (tkeystr, "KEY") == 0) {
      if (key >= 0) {
        llistSetData (df->data, key, itemList);
        key = -1L;
      }
      key = atol (tvalstr);
      itemList = llistAlloc (LIST_ORDERED, free);
      continue;
    }
    if (! first &&
        dftype == DFTYPE_KEY_STRING &&
        strcmp (keyString, tkeystr) == 0) {
      if (ikeystr != NULL) {
        slistSetData (df->data, ikeystr, itemList);
        ikeystr = NULL;
      }
      keyString = tkeystr;
      itemList = slistAlloc (LIST_ORDERED, istringCompare, free, free);
      continue;
    }

    if (dftype == DFTYPE_LIST) {
      listSet (df->data, strdup (tkeystr));
    }
    if (dftype == DFTYPE_KEY_LONG || dftype == DFTYPE_KEY_STRING) {
      if (first && dftype == DFTYPE_KEY_STRING) {
        keyString = tkeystr;
        ikeystr = tvalstr;
      }

      long idx = dfkeyBinarySearch (dfkeys, dfkeycount, tkeystr);
      if (idx >= 0) {
        ikey = dfkeys [idx].itemkey;
        vt = dfkeys [idx].valuetype;

        if (dfkeys [idx].convFunc != NULL) {
          lval = dfkeys [idx].convFunc (tvalstr);
        } else {
          if (vt == VALUE_LONG) {
            lval = atol (tvalstr);
          }
        }
      }

      if (dftype == DFTYPE_KEY_STRING) {
        if (vt == VALUE_DATA) {
          slistSetData (itemList, tkeystr, strdup (tvalstr));
        }
        if (vt == VALUE_LONG) {
          slistSetLong (itemList, tkeystr, lval);
        }
      }
      if (dftype == DFTYPE_KEY_LONG) {
        if (vt == VALUE_DATA) {
          llistSetData (itemList, ikey, strdup (tvalstr));
        }
        if (vt == VALUE_LONG) {
          llistSetLong (itemList, ikey, atol (tvalstr));
        }
      }
    }
    if (dftype == DFTYPE_KEY_VAL) {
      slistSetData (df->data, tkeystr, strdup (tvalstr));
      key = -1L;
    }

    first = 0;
  }

  if (dftype == DFTYPE_KEY_LONG && key >= 0) {
    llistSetData (df->data, key, itemList);
  }
  if (dftype == DFTYPE_KEY_STRING && ikeystr != NULL) {
    slistSetData (df->data, ikeystr, itemList);
  }
  if (dftype == DFTYPE_KEY_STRING) {
    slistSort (df->data);
  }

  parseFree (pi);
  free (data);
}

int
datafileSave (datafilekey_t *dfkeys, size_t dfkeycount, datafile_t *data)
{
/* ### FIX : TODO */
  makeBackups (data->fname, 2);
  return 0;
}

/* internal parse routines */

static size_t
parse (parseinfo_t *pi, char *data, parsetype_t parsetype)
{
  char        *tokptr;
  char        *str;
  size_t      dataCounter;

  tokptr = NULL;
  if (pi->allocCount < 60) {
    pi->allocCount = 60;
    pi->strdata = realloc (pi->strdata, sizeof (char *) * pi->allocCount);
    assert (pi->strdata != NULL);
  }

  str = strtok_r (data, "\n", &tokptr);
  dataCounter = 0;
  while (str != NULL) {
    if (*str == '#') {
      str = strtok_r (NULL, "\n", &tokptr);
      continue;
    }

    if ((size_t) dataCounter >= pi->allocCount) {
      pi->allocCount += 10;
      pi->strdata = realloc (pi->strdata, sizeof (char *) * pi->allocCount);
      assert (pi->strdata != NULL);
    }
    if (parsetype == PARSE_KEYVALUE && dataCounter % 2 == 1) {
      /* value data is indented */
      str += 2;
    }
    pi->strdata [dataCounter] = str;
    ++dataCounter;
    str = strtok_r (NULL, "\n", &tokptr);
  }
  pi->count = dataCounter;
  return dataCounter;
}

/* internal datafile routines */

static void
datafileFreeInternal (datafile_t *df)
{
  if (df != NULL) {
    if (df->fname != NULL) {
      free (df->fname);
      df->fname = NULL;
    }
    if (df->data != NULL) {
      switch (df->dftype) {
        case DFTYPE_LIST: {
          listFree (df->data);
          break;
        }
        case DFTYPE_KEY_STRING: {
          slistFree (df->data);
          break;
        }
        case DFTYPE_KEY_VAL:
        case DFTYPE_KEY_LONG: {
          llistFree (df->data);
          break;
        }
        default: {
          break;
        }
      }
      df->data = NULL;
    }
  }
}

static long
dfkeyBinarySearch (const datafilekey_t *dfkeys, size_t count, char *key)
{
  long      l = 0;
  long      r = (long) count - 1;
  long      m = 0;
  int       rc;

  while (l <= r) {
    m = l + (r - l) / 2;

    rc = stringCompare (dfkeys [m].name, key);
    if (rc == 0) {
      return m;
    }

    if (rc < 0) {
      l = m + 1;
    } else {
      r = m - 1;
    }
  }

  return -1;
}

