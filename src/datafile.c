#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "datafile.h"
#include "tmutil.h"
#include "list.h"
#include "fileop.h"
#include "bdjstring.h"
#include "log.h"
#include "portability.h"
#include "filedata.h"

typedef enum {
  PARSE_SIMPLE,
  PARSE_KEYVALUE
} parsetype_t;

static size_t parse (parseinfo_t *pi, char *data, parsetype_t parsetype);
static void   datafileFreeInternal (datafile_t *df);
static void   datafileCheckDfkeys (char *name, datafilekey_t *dfkeys, size_t dfkeycount);

/* parsing routines */

parseinfo_t *
parseInit (void)
{
  parseinfo_t   *pi;

  logProcBegin (LOG_PROC, "parseInit");
  pi = malloc (sizeof (parseinfo_t));
  assert (pi != NULL);
  pi->strdata = NULL;
  pi->allocCount = 0;
  pi->count = 0;
  logProcEnd (LOG_PROC, "parseInit", "");
  return pi;
}

void
parseFree (parseinfo_t *pi)
{
  logProcBegin (LOG_PROC, "parseFree");
  if (pi != NULL) {
    if (pi->strdata != NULL) {
      free (pi->strdata);
    }
    free (pi);
  }
  logProcEnd (LOG_PROC, "parseFree", "");
}

char **
parseGetData (parseinfo_t *pi)
{
  return pi->strdata;
}

size_t
parseSimple (parseinfo_t *pi, char *data)
{
  logProcBegin (LOG_PROC, "parseSimple");
  logProcEnd (LOG_PROC, "parseSimple", "");
  return parse (pi, data, PARSE_SIMPLE);
}

size_t
parseKeyValue (parseinfo_t *pi, char *data)
{
  logProcBegin (LOG_PROC, "parseKeyValue");
  logProcEnd (LOG_PROC, "parseKeyValue", "");
  return parse (pi, data, PARSE_KEYVALUE);
}

void
parseConvBoolean (char *data, datafileret_t *ret)
{
  logProcBegin (LOG_PROC, "parseConvBoolean");
  ret->valuetype = VALUE_NUM;
  ret->u.num = 0;
  if (strcmp (data, "on") == 0 ||
      strcmp (data, "yes") == 0 ||
      strcmp (data, "1") == 0) {
    ret->u.num = 1;
  }
  logProcEnd (LOG_PROC, "parseConvBoolean", "");
}

void
parseConvTextList (char *data, datafileret_t *ret)
{
  char          *tokptr;
  char          *p;

  logProcBegin (LOG_PROC, "parseConvTextList");
  ret->valuetype = VALUE_LIST;
  ret->u.list = NULL;
  ret->u.list = listAlloc ("textlist", LIST_ORDERED, istringCompare, free, NULL);
  if (data != NULL) {
    p = strtok_r (data, " ,", &tokptr);
    while (p != NULL) {
      listSetData (ret->u.list, strdup (p), NULL);
      p = strtok_r (NULL, " ,", &tokptr);
    }
  }
  logProcEnd (LOG_PROC, "parseConvTextList", "");
}

/* datafile loading routines */

datafile_t *
datafileAlloc (char *name)
{
  datafile_t      *df;

  logProcBegin (LOG_PROC, "datafileAlloc");
  df = malloc (sizeof (datafile_t));
  assert (df != NULL);
  df->name = name;
  df->fname = NULL;
  df->data = NULL;
  df->lookup = NULL;
  df->dftype = DFTYPE_NONE;
  logProcEnd (LOG_PROC, "datafileAlloc", "");
  return df;
}

datafile_t *
datafileAllocParse (char *name, datafiletype_t dftype, char *fname,
    datafilekey_t *dfkeys, size_t dfkeycount, listidx_t lookupKey)
{
  logProcBegin (LOG_PROC, "datafileAllocParse");
  logMsg (LOG_DBG, LOG_DATAFILE, "alloc/parse %s", fname);
  datafile_t *df = datafileAlloc (name);
  if (df != NULL) {
    char *ddata = datafileLoad (df, dftype, fname);
    if (ddata != NULL) {
      df->data = datafileParse (ddata, name, dftype,
          dfkeys, dfkeycount, lookupKey, &df->lookup);
      if (dftype == DFTYPE_KEY_VAL && dfkeys == NULL) {
        listSort (df->data);
      } else if (dftype != DFTYPE_LIST) {
        llistSort (df->data);
      }
      free (ddata);
    }
  }
  logProcEnd (LOG_PROC, "datafileAllocParse", "");
  return df;
}

void
datafileFree (void *tdf)
{
  datafile_t    *df = (datafile_t *) tdf;

  logProcBegin (LOG_PROC, "datafileFree");
  if (df != NULL) {
    datafileFreeInternal (df);
    free (df);
  }
  logProcEnd (LOG_PROC, "datafileFree", "");
}

char *
datafileLoad (datafile_t *df, datafiletype_t dftype, char *fname)
{
  char    *data;

  logProcBegin (LOG_PROC, "datafileLoad");
  logMsg (LOG_DBG, LOG_DATAFILE, "load %s", fname);
  df->dftype = dftype;
  if (df->fname == NULL) {
    df->fname = strdup (fname);
    assert (df->fname != NULL);
  }
    /* load the new filename */
  data = filedataReadAll (fname);
  logProcEnd (LOG_PROC, "datafileLoad", "");
  return data;
}

list_t *
datafileParse (char *data, char *name, datafiletype_t dftype,
    datafilekey_t *dfkeys, size_t dfkeycount, listidx_t lookupKey,
    list_t **lookup)
{
  list_t        *nlist = NULL;

  nlist = datafileParseMerge (nlist, data, name, dftype,
      dfkeys, dfkeycount, lookupKey, lookup);
  return nlist;
}

list_t *
datafileParseMerge (list_t *nlist, char *data, char *name,
    datafiletype_t dftype, datafilekey_t *dfkeys,
    size_t dfkeycount, listidx_t lookupKey, list_t **lookup)
{
  char          **strdata = NULL;
  parseinfo_t   *pi = NULL;
  listidx_t     key = -1L;
  size_t        dataCount;
  list_t        *itemList = NULL;
  valuetype_t   vt = 0;
  size_t        inc = 2;
  int           first = 1;
  listidx_t     ikey = 0;
  ssize_t       lval = 0;
  double        dval = 0.0;
  char          *tkeystr;
  char          *tvalstr;
  char          *tlookupkey = NULL;
  datafileret_t ret;

  logProcBegin (LOG_PROC, "datafileParseMerge");
  logMsg (LOG_DBG, LOG_DATAFILE, "begin parse %s", name);

  if (dfkeys != NULL) {
    datafileCheckDfkeys (name, dfkeys, dfkeycount);
  }

  pi = parseInit ();
  if (dftype == DFTYPE_LIST) {
    dataCount = parseSimple (pi, data);
  } else {
    dataCount = parseKeyValue (pi, data);
  }
  strdata = parseGetData (pi);

  logMsg (LOG_DBG, LOG_DATAFILE, "dftype: %ld", dftype);
  switch (dftype) {
    case DFTYPE_LIST: {
      inc = 1;
      if (nlist == NULL) {
        nlist = listAlloc (name, LIST_UNORDERED, NULL, free, NULL);
        listSetSize (nlist, dataCount);
      } else {
        listSetSize (nlist, dataCount + listGetCount (nlist));
      }
      break;
    }
    case DFTYPE_INDIRECT: {
      inc = 2;
      if (nlist == NULL) {
        nlist = llistAlloc (name, LIST_UNORDERED, listFree);
      }
      break;
    }
    case DFTYPE_KEY_VAL: {
      inc = 2;
      if (dfkeys == NULL) {
        if (nlist == NULL) {
          nlist = listAlloc (name, LIST_UNORDERED,
              istringCompare, free, free);
          listSetSize (nlist, dataCount / 2);
        } else {
          listSetSize (nlist, dataCount / 2 + listGetCount (nlist));
        }
        logMsg (LOG_DBG, LOG_DATAFILE, "key_val: list");
      } else {
        if (nlist == NULL) {
          nlist = llistAlloc (name, LIST_UNORDERED, free);
          llistSetSize (nlist, dataCount / 2);
        } else {
          llistSetSize (nlist, dataCount / 2 + listGetCount (nlist));
        }
        logMsg (LOG_DBG, LOG_DATAFILE, "key_val: llist");
      }
      break;
    }
    default: {
      break;
    }
  }

  if (lookupKey != DATAFILE_NO_LOOKUP) {
    char      temp [80];

    logMsg (LOG_DBG, LOG_DATAFILE, "with lookup: key:%ld", lookupKey);
    snprintf (temp, sizeof (temp), "%s-lookup", name);
    *lookup = listAlloc (temp, LIST_UNORDERED, istringCompare, free, NULL);
    listSetSize (*lookup, listGetCount (nlist));
  }

  if (dfkeys != NULL) {
    logMsg (LOG_DBG, LOG_DATAFILE, "use dfkeys");
  }

  for (size_t i = 0; i < dataCount; i += inc) {
    tkeystr = strdata [i];
    tvalstr = strdata [i + 1];

    if (inc == 2 && strcmp (strdata [i], "version") == 0) {
//      version = atol (tvalstr);
// ### FIX TOD: want to save version somewhere...
      continue;
    }
    if (strcmp (strdata [i], "count") == 0) {
      if (dftype == DFTYPE_INDIRECT) {
        llistSetSize (nlist, atol (tvalstr));
      }
      listSetSize (*lookup, listGetCount (nlist));
      continue;
    }

    if (dftype == DFTYPE_INDIRECT &&
        strcmp (tkeystr, "KEY") == 0) {
      char      temp [80];

      if (key >= 0) {
        llistSetList (nlist, key, itemList);
        if (lookup != NULL &&
            lookupKey != DATAFILE_NO_LOOKUP &&
            tlookupkey != NULL) {
          logMsg (LOG_DBG, LOG_DATAFILE, "lookup: set %s to %ld", tlookupkey, key);
          listSetNum (*lookup, tlookupkey, key);
        }
        key = -1L;
      }
      key = atol (tvalstr);
      snprintf (temp, sizeof (temp), "%s-item-%ld", name, key);
      itemList = llistAlloc (temp, LIST_ORDERED, free);
      continue;
    }

    if (dftype == DFTYPE_LIST) {
      logMsg (LOG_DBG, LOG_DATAFILE, "set: list");
      listSetData (nlist, tkeystr, NULL);
    }
    if (dftype == DFTYPE_INDIRECT ||
        (dftype == DFTYPE_KEY_VAL && dfkeys != NULL)) {
      listidx_t idx = dfkeyBinarySearch (dfkeys, dfkeycount, tkeystr);
      if (idx >= 0) {
        logMsg (LOG_DBG, LOG_DATAFILE, "found %s idx: %ld", tkeystr, idx);
        ikey = dfkeys [idx].itemkey;
        vt = dfkeys [idx].valuetype;
        logMsg (LOG_DBG, LOG_DATAFILE, "ikey:%ld vt:%d tvalstr:%s", ikey, vt, tvalstr);

        ret.valuetype = VALUE_NONE;
        if (dfkeys [idx].convFunc != NULL) {
          dfkeys [idx].convFunc (tvalstr, &ret);
          vt = ret.valuetype;
          if (vt == VALUE_NUM) {
            lval = ret.u.num;
            logMsg (LOG_DBG, LOG_DATAFILE, "converted value: %s to %ld", tvalstr, lval);
          }
        } else {
          if (vt == VALUE_NUM) {
            lval = atol (tvalstr);
            logMsg (LOG_DBG, LOG_DATAFILE, "value: %ld", lval);
          }
          if (vt == VALUE_DOUBLE) {
            dval = atof (tvalstr);
            logMsg (LOG_DBG, LOG_DATAFILE, "value: %.2f", dval);
          }
        }
      } else {
        logMsg (LOG_DBG, LOG_DATAFILE, "ERR: Unable to locate key: %s", tkeystr);
      }

      if (lookup != NULL &&
          lookupKey != DATAFILE_NO_LOOKUP &&
          ikey == lookupKey) {
        tlookupkey = tvalstr;
      }

      logMsg (LOG_DBG, LOG_DATAFILE, "set: dftype:%ld vt:%d", dftype, vt);

        /* dftype_indirect has a key pointing to a key/val list. */
      if (dftype == DFTYPE_INDIRECT) {
        if (vt == VALUE_DATA) {
          llistSetData (itemList, ikey, strdup (tvalstr));
        }
        if (vt == VALUE_NUM) {
          llistSetNum (itemList, ikey, lval);
        }
        if (vt == VALUE_DOUBLE) {
          llistSetDouble (itemList, ikey, dval);
        }
        if (vt == VALUE_LIST) {
          llistSetList (itemList, ikey, ret.u.list);
        }
      }
        /* a key/val type has a single list keyed by a long (dfkeys is set) */
      if (dftype == DFTYPE_KEY_VAL) {
        if (vt == VALUE_DATA) {
          llistSetData (nlist, ikey, strdup (tvalstr));
        }
        if (vt == VALUE_NUM) {
          llistSetNum (nlist, ikey, lval);
        }
        if (vt == VALUE_DOUBLE) {
          llistSetDouble (nlist, ikey, dval);
        }
        if (vt == VALUE_LIST) {
          llistSetList (nlist, ikey, ret.u.list);
        }
      }
    }
    if (dftype == DFTYPE_KEY_VAL && dfkeys == NULL) {
      logMsg (LOG_DBG, LOG_DATAFILE, "set: key_val");
      listSetData (nlist, tkeystr, strdup (tvalstr));
      key = -1L;
    }

    first = 0;
  }

  if (dftype == DFTYPE_INDIRECT && key >= 0) {
    llistSetList (nlist, key, itemList);
    if (lookup != NULL &&
        lookupKey != DATAFILE_NO_LOOKUP &&
        tlookupkey != NULL) {
      listSetNum (*lookup, tlookupkey, key);
    }
  }

  if (lookupKey != DATAFILE_NO_LOOKUP) {
    listSort (*lookup);
    listStartIterator (*lookup);
    while ((tkeystr = listIterateKeyStr (*lookup)) != NULL) {
      ikey = listIterateGetIdx (*lookup);
      lval = listGetNumByIdx (*lookup, ikey);
      logMsg (LOG_DBG, LOG_DATAFILE | LOG_BASIC,
          "%s: %s / %zd", name, tkeystr, lval);
    }
  }

  parseFree (pi);
  logProcEnd (LOG_PROC, "datafileParseMerge", "");
  return nlist;
}

listidx_t
dfkeyBinarySearch (const datafilekey_t *dfkeys, ssize_t count, char *key)
{
  listidx_t     l = 0;
  listidx_t     r = count - 1;
  listidx_t     m = 0;
  int           rc;

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

list_t *
datafileGetList (datafile_t *df)
{
  if (df != NULL) {
    return df->data;
  }
  return NULL;
}

list_t *
datafileGetLookup (datafile_t *df)
{
  if (df != NULL) {
    return df->lookup;
  }
  return NULL;
}

void
datafileSetData (datafile_t *df, void *data)
{
  if (df != NULL) {
    return;
  }
  df->data = data;
  return;
}

int
datafileSave (datafilekey_t *dfkeys, size_t dfkeycount, datafile_t *data)
{
/* ### FIX : TODO */
  datafileBackup (data->fname, 2);
  return 0;
}

void
datafileBackup (char *fname, int count)
{
  char      ofn [MAXPATHLEN];
  char      nfn [MAXPATHLEN];

  for (int i = count; i >= 1; --i) {
    snprintf (nfn, MAXPATHLEN, "%s.bak.%d", fname, i);
    snprintf (ofn, MAXPATHLEN, "%s.bak.%d", fname, i - 1);
    if (i - 1 == 0) {
      strlcpy (ofn, fname, MAXPATHLEN);
    }
    if (fileopExists (ofn)) {
      if ((i - 1) != 0) {
        fileopMove (ofn, nfn);
      } else {
        fileopCopy (ofn, nfn);
      }
    }
  }
  return;
}

/* internal parse routines */

static size_t
parse (parseinfo_t *pi, char *data, parsetype_t parsetype)
{
  char        *tokptr;
  char        *str;
  ssize_t     dataCounter;

  logProcBegin (LOG_PROC, "parse");
  tokptr = NULL;
  if (data == NULL) {
    logProcEnd (LOG_PROC, "parse", "null-data");
    return 0;
  }

  if (pi->allocCount < 60) {
    pi->allocCount = 60;
    pi->strdata = realloc (pi->strdata, sizeof (char *) * pi->allocCount);
    assert (pi->strdata != NULL);
  }

  dataCounter = 0;
  str = strtok_r (data, "\n", &tokptr);
  while (str != NULL) {
    if (*str == '#') {
      str = strtok_r (NULL, "\n", &tokptr);
      continue;
    }

    if (dataCounter >= pi->allocCount) {
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
  logProcEnd (LOG_PROC, "parse", "");
  return dataCounter;
}

/* internal datafile routines */

static void
datafileFreeInternal (datafile_t *df)
{
  logProcBegin (LOG_PROC, "datafileFreeInternal");
  if (df != NULL) {
    if (df->fname != NULL) {
      free (df->fname);
      df->fname = NULL;
    }
    if (df->lookup != NULL) {
      listFree (df->lookup);
    }
    if (df->data != NULL) {
      switch (df->dftype) {
        case DFTYPE_LIST: {
          listFree (df->data);
          break;
        }
        case DFTYPE_INDIRECT: {
          llistFree (df->data);
          break;
        }
        case DFTYPE_KEY_VAL:
        {
          list_t    *tlist;

          tlist = df->data;
          if (tlist->keytype == KEY_STR) {
            listFree (df->data);
          } else {
            llistFree (df->data);
          }
          break;
        }
        default: {
          break;
        }
      }
      df->data = NULL;
    }
  }
  logProcEnd (LOG_PROC, "datafileFreeInternal", "");
}

static void
datafileCheckDfkeys (char *name, datafilekey_t *dfkeys, size_t dfkeycount)
{
  char          *last = "";

  for (size_t i = 0; i < dfkeycount; ++i) {
    if (strcmp (dfkeys [i].name, last) <= 0) {
      fprintf (stderr, "datafile: %s dfkey out of order: %s\n", name, dfkeys [i].name);
      logMsg (LOG_DBG, LOG_IMPORTANT, "ERR: datafile: %s dfkey out of order: %s", name, dfkeys [i].name);
    }
    last = dfkeys [i].name;
  }
}
