#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "bdjstring.h"
#include "istring.h"
#include "list.h"
#include "log.h"
#include "tmutil.h"

static void     listFreeItem (list_t *, listidx_t);
static listidx_t listIterateKeyGetNum (list_t *list, listidx_t *iteridx);
static void     listInsert (list_t *, listidx_t loc, listitem_t *item);
static void     listReplace (list_t *, listidx_t, listitem_t *item);
static int      listBinarySearch (const list_t *, listkeylookup_t *key, listidx_t *);
static int      idxCompare (listidx_t, listidx_t);
static int      listCompare (const list_t *, const listkey_t *a, const listkey_t *b);
static long     merge (list_t *, listidx_t, listidx_t, listidx_t);
static long     mergeSort (list_t *, listidx_t, listidx_t);

list_t *
listAlloc (const char *name, listorder_t ordered, listFree_t valueFreeHook)
{
  list_t    *list;

  logProcBegin (LOG_PROC, "listAlloc");
  list = malloc (sizeof (list_t));
  assert (list != NULL);
    /* always allocate the name so that dynamic names can be created */
  list->name = strdup (name);
  assert (list->name != NULL);
  list->data = NULL;
  list->version = 1;
  list->count = 0;
  list->allocCount = 0;
  list->maxKeyWidth = 0;
  list->maxDataWidth = 0;
  list->replace = false;
  list->keytype = LIST_KEY_STR;
  list->ordered = ordered;
  list->keyCache.strkey = NULL;
  list->locCache = LIST_LOC_INVALID;
  list->readCacheHits = 0;
  list->writeCacheHits = 0;
  list->valueFreeHook = valueFreeHook;
  logMsg (LOG_DBG, LOG_LIST, "alloc %s", name);
  logProcEnd (LOG_PROC, "listAlloc", "");
  return list;
}

void
listFree (void *tlist)
{
  list_t *list = (list_t *) tlist;

  if (list != NULL) {
    logMsg (LOG_DBG, LOG_LIST, "free list %s", list->name);
    if (list->readCacheHits > 0 || list->writeCacheHits > 0) {
      logMsg (LOG_DBG, LOG_LIST,
          "list %s: cache read:%ld write:%ld",
          list->name, list->readCacheHits, list->writeCacheHits);
    }
    if (list->data != NULL) {
      for (listidx_t i = 0; i < list->count; ++i) {
        listFreeItem (list, i);
      }
      free (list->data);
      list->data = NULL;
    } /* data is not null */

    list->count = 0;
    list->allocCount = 0;
    if (list->keytype == LIST_KEY_STR &&
        list->keyCache.strkey != NULL) {
      free (list->keyCache.strkey);
      list->keyCache.strkey = NULL;
      list->locCache = LIST_LOC_INVALID;
    }
    if (list->name != NULL) {
      free (list->name);
      list->name = NULL;
    }
    free (list);
  }
}

inline void
listSetVersion (list_t *list, int version)
{
  if (list == NULL) {
    return;
  }
  list->version = version;
}

void
listSetSize (list_t *list, listidx_t siz)
{
  if (list == NULL) {
    logMsg (LOG_DBG, LOG_IMPORTANT, "listsetsize: null list");
    return;
  }

  if (siz > list->allocCount) {
    listidx_t   tsiz;

    tsiz = list->allocCount;
    list->allocCount = siz;
    list->data = realloc (list->data,
        (size_t) list->allocCount * sizeof (listitem_t));
    assert (list->data != NULL);
    memset (list->data + tsiz, '\0', sizeof (listitem_t) * (siz - tsiz));
  }
}

inline int
listGetVersion (list_t *list)
{
  if (list == NULL) {
    return LIST_NO_VERSION;
  }
  return list->version;
}

void *
listGetData (list_t *list, const char *keydata)
{
  void            *value = NULL;
  listkeylookup_t key;
  listidx_t       idx;

  if (list == NULL) {
    return value;
  }
  if (keydata == NULL) {
    return value;
  }

  key.strkey = keydata;
  idx = listGetIdx (list, &key);
  if (idx >= 0) {
    value = list->data [idx].value.data;
  }
  logMsg (LOG_DBG, LOG_LIST, "list:%s key:%s idx:%ld", list->name, keydata, idx);
  return value;
}

void *
listGetDataByIdx (list_t *list, listidx_t idx)
{
  void  *value = NULL;


  if (list == NULL) {
    return value;
  }
  if (idx < 0 || idx >= list->count) {
    return value;
  }
  value = list->data [idx].value.data;
  logMsg (LOG_DBG, LOG_LIST, "list:%s idx:%ld", list->name, idx);
  return value;
}

ssize_t
listGetNumByIdx (list_t *list, listidx_t idx)
{
  ssize_t     value = LIST_VALUE_INVALID;

  if (list == NULL) {
    return value;
  }
  if (idx < 0 || idx >= list->count) {
    return value;
  }
  value = list->data [idx].value.num;
  logMsg (LOG_DBG, LOG_LIST, "list:%s idx:%ld", list->name, idx);
  return value;
}

ssize_t
listGetNum (list_t *list, const char *keydata)
{
  ssize_t         value = LIST_VALUE_INVALID;
  listkeylookup_t key;
  listidx_t       idx;

  if (list == NULL) {
    return value;
  }
  if (keydata == NULL) {
    return value;
  }

  key.strkey = keydata;
  idx = listGetIdx (list, &key);
  if (idx >= 0) {
    value = list->data [idx].value.num;
  }
  logMsg (LOG_DBG, LOG_LIST, "list:%s key:%s idx:%ld value:%ld", list->name, keydata, idx, value);
  return value;
}

void
listDeleteByIdx (list_t *list, listidx_t idx)
{
  listidx_t   copycount;

  if (list == NULL) {
    return;
  }
  if (idx < 0 || idx >= list->count) {
    return;
  }

  copycount = list->count - idx - 1;
  listFreeItem (list, idx);
  list->count -= 1;

  if (list->keytype == LIST_KEY_STR &&
      list->keyCache.strkey != NULL) {
    free (list->keyCache.strkey);
    list->keyCache.strkey = NULL;
  }
  list->locCache = LIST_LOC_INVALID;

  if (copycount > 0) {
    for (listidx_t i = idx; i < list->count; ++i) {
       memcpy (list->data + i, list->data + i + 1, sizeof (listitem_t));
    }
  }
  logMsg (LOG_DBG, LOG_LIST, "list-del:%s idx:%ld", list->name, idx);
}

void
listSort (list_t *list)
{
  mstime_t      tm;
  time_t        elapsed;
  long          swaps;

  mstimestart (&tm);
  list->ordered = LIST_ORDERED;
  swaps = mergeSort (list, 0, list->count - 1);
  elapsed = mstimeend (&tm);
  if (elapsed > 0) {
    logMsg (LOG_DBG, LOG_LIST, "sort of %s took %ld ms with %ld swaps", list->name, elapsed, swaps);
  }
}

inline void
listStartIterator (list_t *list, listidx_t *iteridx)
{
  *iteridx = LIST_END_LIST;
}

inline void
listDumpInfo (list_t *list)
{
  logMsg (LOG_DBG, LOG_LIST, "list: %s count: %ld key:%d ordered:%d",
      list->name, list->count, list->keytype, list->ordered);
}

listidx_t
listIterateKeyNum (list_t *list, listidx_t *iteridx)
{
  logProcBegin (LOG_PROC, "listIterateKeyNum");
  if (list == NULL || list->keytype == LIST_KEY_STR) {
    logProcEnd (LOG_PROC, "listIterateKeyNum", "null-list/key-str");
    return LIST_LOC_INVALID;
  }

  ++(*iteridx);

  logProcEnd (LOG_PROC, "listIterateKeyNum", "");
  return listIterateKeyGetNum (list, iteridx);
}

listidx_t
listIterateKeyPreviousNum (list_t *list, listidx_t *iteridx)
{
  logProcBegin (LOG_PROC, "listIterateKeyPreviousNum");
  if (list == NULL || list->keytype == LIST_KEY_STR) {
    logProcEnd (LOG_PROC, "listIterateKeyNum", "null-list/key-str");
    return LIST_LOC_INVALID;
  }

  --(*iteridx);
  if (*iteridx < 0) {
    /* do not decrement further! */
    *iteridx = -1;
  }

  logProcEnd (LOG_PROC, "listIterateKeyPreviousNum", "");
  return listIterateKeyGetNum (list, iteridx);
}

char *
listIterateKeyStr (list_t *list, listidx_t *iteridx)
{
  char    *value = NULL;

  logProcBegin (LOG_PROC, "listIterateKeyStr");
  if (list == NULL || list->keytype == LIST_KEY_NUM) {
    logProcEnd (LOG_PROC, "listIterateKeyStr", "null-list/key-num");
    return NULL;
  }

  ++(*iteridx);
  if (*iteridx >= list->count) {
    *iteridx = LIST_END_LIST;
    logProcEnd (LOG_PROC, "listIterateKeyStr", "end-list");
    return NULL;
  }

  value = list->data [*iteridx].key.strkey;

  if (list->keyCache.strkey != NULL) {
    free (list->keyCache.strkey);
    list->keyCache.strkey = NULL;
    list->locCache = LIST_LOC_INVALID;
  }

  list->keyCache.strkey = strdup (value);
  list->locCache = *iteridx;

  logProcEnd (LOG_PROC, "listIterateKeyStr", "");
  return value;
}

void *
listIterateValue (list_t *list, listidx_t *iteridx)
{
  void  *value = NULL;

  logProcBegin (LOG_PROC, "listIterateValue");
  if (list == NULL) {
    logProcEnd (LOG_PROC, "listIterateValue", "null-list");
    return NULL;
  }

  ++(*iteridx);
  if (*iteridx >= list->count) {
    *iteridx = LIST_END_LIST;
    logProcEnd (LOG_PROC, "listIterateValue", "end-list");
    return NULL;  /* indicate the end of the list */
  }

  value = list->data [*iteridx].value.data;
  logProcEnd (LOG_PROC, "listIterateValue", "");
  return value;
}

ssize_t
listIterateValueNum (list_t *list, listidx_t *iteridx)
{
  ssize_t     value = LIST_VALUE_INVALID;

  logProcBegin (LOG_PROC, "listIterateNum");
  if (list == NULL) {
    logProcEnd (LOG_PROC, "listIterateNum", "null-list");
    return LIST_VALUE_INVALID;
  }

  ++(*iteridx);
  if (*iteridx >= list->count) {
    *iteridx = LIST_END_LIST;
    logProcEnd (LOG_PROC, "listIterateNum", "end-list");
    return LIST_VALUE_INVALID;  /* indicate the end of the list */
  }

  value = list->data [*iteridx].value.num;
  logProcEnd (LOG_PROC, "listIterateNum", "");
  return value;
}

listidx_t
listIterateGetIdx (list_t *list, listidx_t *iteridx)
{
  if (list == NULL) {
    return LIST_LOC_INVALID;
  }

  return *iteridx;
}

listidx_t
listGetIdx (list_t *list, listkeylookup_t *key)
{
  listidx_t   idx;
  listidx_t   ridx;
  int         rc;

  if (list == NULL) {
    return LIST_LOC_INVALID;
  }

  /* check the cache */
  if (list->locCache >= 0L) {
    if ((list->keytype == LIST_KEY_STR &&
         key->strkey != NULL &&
         list->keyCache.strkey != NULL &&
         strcmp (key->strkey, list->keyCache.strkey) == 0) ||
        (list->keytype == LIST_KEY_NUM &&
         key->idx == list->keyCache.idx)) {
      ridx = list->locCache;
      ++list->readCacheHits;
      return ridx;
    }
  }

  ridx = LIST_LOC_INVALID;
  if (list->ordered == LIST_ORDERED) {
    rc = listBinarySearch (list, key, &idx);
    if (rc == 0) {
      ridx = idx;
    }
  } else if (list->replace) {
    for (listidx_t i = 0; i < list->count; ++i) {
      if (list->keytype == LIST_KEY_STR) {
        if (strcmp (list->data [i].key.strkey, key->strkey) == 0) {
          ridx = i;
          break;
        }
      } else {
        if (list->data [i].key.idx == key->idx) {
          ridx = i;
          break;
        }
      }
    }
  }

  if (list->keytype == LIST_KEY_STR &&
      list->keyCache.strkey != NULL) {
    free (list->keyCache.strkey);
    list->keyCache.strkey = NULL;
  }
  list->locCache = LIST_LOC_INVALID;

  if (ridx >= 0) {
    if (list->keytype == LIST_KEY_STR) {
      list->keyCache.strkey = strdup (key->strkey);
    }
    if (list->keytype == LIST_KEY_NUM) {
      list->keyCache.idx = key->idx;
    }
    list->locCache = ridx;
  }
  return ridx;
}

void
listSet (list_t *list, listitem_t *item)
{
  listidx_t       loc = 0L;
  int             rc = -1;
  int             found = 0;

  if (list == NULL) {
    return;
  }

  if (list->locCache >= 0L) {
    if ((list->keytype == LIST_KEY_STR &&
         item->key.strkey != NULL &&
         list->keyCache.strkey != NULL &&
         strcmp (item->key.strkey, list->keyCache.strkey) == 0) ||
        (list->keytype == LIST_KEY_NUM &&
         item->key.idx == list->keyCache.idx)) {
      loc = list->locCache;
      ++list->writeCacheHits;
      found = 1;
      rc = 0;
    }
  }

  if (! found && list->count > 0) {
    if (list->ordered == LIST_ORDERED) {
      rc = listBinarySearch (list, (listkeylookup_t *) &item->key, &loc);
    } else {
      loc = list->count;
    }
  }

  if (list->ordered == LIST_ORDERED) {
    if (rc < 0) {
      listInsert (list, loc, item);
    } else {
      listReplace (list, loc, item);
    }
  } else {
    listInsert (list, loc, item);
  }
  return;
}

/* used by the test suite */
bool
listDebugIsCached (list_t *list, listidx_t key)
{
  bool  rc;

  if (list == NULL) {
    return false;
  }

  rc = list->locCache != LIST_LOC_INVALID &&
      list->keyCache.idx == key;
  return rc;
}

/* internal routines */

static void
listFreeItem (list_t *list, listidx_t idx)
{
  listitem_t    *dp;

  if (list == NULL) {
    return;
  }
  if (list->data == NULL) {
    return;
  }
  if (idx >= list->count) {
    return;
  }

  dp = &list->data [idx];

  if (dp != NULL) {
    if (list->keytype == LIST_KEY_STR &&
        dp->key.strkey != NULL) {
      free (dp->key.strkey);
      dp->key.strkey = NULL;
    }
    if (dp->valuetype == VALUE_STR &&
        dp->value.data != NULL) {
      free (dp->value.data);
      dp->value.data = NULL;
    }
    if (dp->valuetype == VALUE_DATA &&
        dp->value.data != NULL &&
        list->valueFreeHook != NULL) {
      list->valueFreeHook (dp->value.data);
    }
    if (dp->valuetype == VALUE_LIST &&
        dp->value.data != NULL) {
      listFree (dp->value.data);
    }
  } /* if the data pointer is not null */
}

inline static listidx_t
listIterateKeyGetNum (list_t *list, listidx_t *iteridx)
{
  listidx_t   value = LIST_LOC_INVALID;

  logProcBegin (LOG_PROC, "listIterateKeyGetNum");
  if (*iteridx < 0 || *iteridx >= list->count) {
    *iteridx = LIST_END_LIST;
    logProcEnd (LOG_PROC, "listIterateKeyGetNum", "end-list");
    return LIST_LOC_INVALID;      /* indicate the beg/end of the list */
  }

  value = list->data [*iteridx].key.idx;

  list->keyCache.idx = value;
  list->locCache = *iteridx;
  logProcEnd (LOG_PROC, "listIterateKeyGetNum", "");
  return value;
}

static void
listInsert (list_t *list, listidx_t loc, listitem_t *item)
{
  listidx_t      copycount;

  ++list->count;
  if (list->count > list->allocCount) {
    ++list->allocCount;
    list->data = realloc (list->data,
        (size_t) list->allocCount * sizeof (listitem_t));
    assert (list->data != NULL);
  }

  assert (list->data != NULL);
  assert ((list->count > 0 && loc < list->count) ||
          (list->count == 0 && loc == 0));

  copycount = list->count - (loc + 1);
  if (copycount > 0) {
    for (listidx_t i = copycount - 1; i >= 0; --i) {
       memcpy (list->data + loc + 1 + i, list->data + loc + i, sizeof (listitem_t));
    }
  }
  memcpy (&list->data [loc], item, sizeof (listitem_t));
}

static void
listReplace (list_t *list, listidx_t loc, listitem_t *item)
{
  assert (list->data != NULL);
  assert ((list->count > 0 && loc < list->count) ||
          (list->count == 0 && loc == 0));

  listFreeItem (list, loc);
  memcpy (&list->data [loc], item, sizeof (listitem_t));
}

static int
idxCompare (listidx_t la, listidx_t lb)
{
  int rc = 0;

  if (la < lb) {
    rc = -1;
  } else if (la > lb) {
    rc = 1;
  }
  return rc;
}

static int
listCompare (const list_t *list, const listkey_t *a, const listkey_t *b)
{
  int         rc;

  rc = 0;
  if (list->ordered == LIST_UNORDERED) {
    rc = 0;
  } else {
    if (list->keytype == LIST_KEY_STR) {
      rc = istringCompare (a->strkey, b->strkey);
    }
    if (list->keytype == LIST_KEY_NUM) {
      rc = idxCompare (a->idx, b->idx);
    }
  }
  return rc;
}

/* returns the location after as a negative number if not found */
static int
listBinarySearch (const list_t *list, listkeylookup_t *key, listidx_t *loc)
{
  listidx_t     l = 0;
  listidx_t     r = list->count - 1;
  listidx_t     m = 0;
  listidx_t     rm;
  int           rc;

  rm = 0;
  while (l <= r) {
    m = l + (r - l) / 2;

    rc = listCompare (list, &list->data [m].key, (listkey_t *) key);
    if (rc == 0) {
      *loc = (listidx_t) m;
      return 0;
    }

    if (rc < 0) {
      l = m + 1;
      rm = l;
    } else {
      r = m - 1;
      rm = m;
    }
  }

  *loc = (listidx_t) rm;
  return -1;
}

/*
 * in-place merge sort from:
 * https://www.geeksforgeeks.org/in-place-merge-sort/
 */

static long
merge (list_t *list, listidx_t start, listidx_t mid, listidx_t end)
{
  listidx_t   start2 = mid + 1;
  listitem_t  value;
  long        swaps = 0;

  int rc = listCompare (list, &list->data [mid].key, &list->data [start2].key);
  if (rc <= 0) {
    return swaps;
  }

  while (start <= mid && start2 <= end) {
    rc = listCompare (list, &list->data [start].key, &list->data [start2].key);
    if (rc <= 0) {
      start++;
    } else {
      listidx_t       index;

      ++swaps;
      value = list->data [start2];
      index = start2;

      while (index != start) {
        list->data [index] = list->data [index - 1];
        index--;
      }
      list->data [start] = value;

      start++;
      mid++;
      start2++;
    }
  }

  return swaps;
}

static long
mergeSort (list_t *list, listidx_t l, listidx_t r)
{
  long swaps = 0;

  if (list->count > 0 && l < r) {
    listidx_t m = l + (r - l) / 2;
    swaps += mergeSort (list, l, m);
    swaps += mergeSort (list, m + 1, r);
    swaps += merge (list, l, m, r);
  }

  return swaps;
}

