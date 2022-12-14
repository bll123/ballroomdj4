#include "config.h"


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "bdjstring.h"
#include "ilist.h"
#include "list.h"
#include "log.h"
#include "nlist.h"
#include "slist.h"

static void ilistSetDataValType (ilist_t *list, ilistidx_t ikey, ilistidx_t lidx,
    void *data, valuetype_t valueType);
static void ilistSetDataItem (ilist_t *list, ilistidx_t ikey, ilistidx_t lidx,
    listitem_t *item);

/* key/value list, keyed by a ilistidx_t */

ilist_t *
ilistAlloc (const char *name, ilistorder_t ordered)
{
  ilist_t    *list;

  list = listAlloc (name, ordered, listFree);
  list->keytype = LIST_KEY_NUM;
  return list;
}

inline void
ilistFree (void *list)
{
  listFree (list);
}

inline void
ilistSetVersion (ilist_t *list, int version)
{
  listSetVersion (list, version);
}

inline int
ilistGetVersion (ilist_t *list)
{
  return listGetVersion (list);
}

inline ilistidx_t
ilistGetCount (ilist_t *list)
{
  if (list == NULL) {
    return 0;
  }
  return list->count;
}

inline void
ilistSetSize (ilist_t *list, ilistidx_t siz)
{
  listSetSize (list, siz);
}

void
ilistSetStr (ilist_t *list, ilistidx_t ikey, ilistidx_t lidx, const char *data)
{
  char *tdata = NULL;

  if (data != NULL) {
    tdata = strdup (data);
  }
  ilistSetDataValType (list, ikey, lidx, tdata, VALUE_STR);
}

void
ilistSetList (ilist_t *list, ilistidx_t ikey, ilistidx_t lidx, void *data)
{
  ilistSetDataValType (list, ikey, lidx, data, VALUE_LIST);
}

void
ilistSetData (ilist_t *list, ilistidx_t ikey, ilistidx_t lidx, void *data)
{
  ilistSetDataValType (list, ikey, lidx, data, VALUE_DATA);
}

void
ilistSetNum (ilist_t *list, ilistidx_t ikey, ilistidx_t lidx, ilistidx_t data)
{
  listitem_t    item;

  if (list == NULL) {
    return;
  }

  item.key.idx = lidx;
  item.valuetype = VALUE_NUM;
  item.value.num = data;
  ilistSetDataItem (list, ikey, lidx, &item);
}

void
ilistSetDouble (ilist_t *list, ilistidx_t ikey, ilistidx_t lidx, double data)
{
  listitem_t    item;

  if (list == NULL) {
    return;
  }

  item.key.idx = lidx;
  item.valuetype = VALUE_DOUBLE;
  item.value.dval = data;

  ilistSetDataItem (list, ikey, lidx, &item);
}

bool
ilistExists (list_t *list, ilistidx_t ikey)
{
  ilist_t   *datalist = NULL;

  if (list == NULL) {
    return false;
  }

  datalist = nlistGetList (list, ikey);
  if (datalist == NULL) {
    return false;
  }

  return true;
}

void *
ilistGetData (ilist_t *list, ilistidx_t ikey, ilistidx_t lidx)
{
  void            *value = NULL;
  listkeylookup_t  key;
  ilistidx_t       idx = 0;
  ilist_t          *datalist = NULL;

  if (list == NULL) {
    return NULL;
  }

  datalist = nlistGetList (list, ikey);
  if (datalist != NULL) {
    key.idx = lidx;
    idx = listGetIdx (datalist, &key);
    if (idx >= 0) {
      value = datalist->data [idx].value.data;
    }
    logMsg (LOG_DBG, LOG_LIST, "ilist:%s key:%ld idx:%ld", datalist->name, lidx, idx);
  }
  return value;
}

char *
ilistGetStr (ilist_t *list, ilistidx_t ikey, ilistidx_t lidx)
{
  return ilistGetData (list, ikey, lidx);
}

ilistidx_t
ilistGetNum (ilist_t *list, ilistidx_t ikey, ilistidx_t lidx)
{
  ilistidx_t      value = LIST_VALUE_INVALID;
  listkeylookup_t key;
  ilistidx_t      idx;
  nlist_t         *datalist;

  if (list == NULL) {
    return LIST_VALUE_INVALID;
  }

  datalist = nlistGetList (list, ikey);
  if (datalist != NULL) {
    key.idx = lidx;
    idx = listGetIdx (datalist, &key);
    if (idx >= 0) {
      value = datalist->data [idx].value.num;
    }
    logMsg (LOG_DBG, LOG_LIST, "list:%s key:%ld idx:%ld value:%ld", list->name, lidx, idx, value);
  }
  return value;
}

double
ilistGetDouble (ilist_t *list, ilistidx_t ikey, ilistidx_t lidx)
{
  double          value = LIST_DOUBLE_INVALID;
  listkeylookup_t key;
  ilistidx_t      idx;
  nlist_t         *datalist;

  if (list == NULL) {
    return LIST_DOUBLE_INVALID;
  }

  datalist = nlistGetList (list, ikey);
  if (datalist != NULL) {
    key.idx = lidx;
    idx = listGetIdx (datalist, &key);
    if (idx >= 0) {
      value = datalist->data [idx].value.dval;
    }
    logMsg (LOG_DBG, LOG_LIST, "list:%s key:%ld idx:%ld value:%8.2g", list->name, lidx, idx, value);
  }
  return value;
}

slist_t *
ilistGetList (ilist_t *list, ilistidx_t ikey, ilistidx_t lidx)
{
  return ilistGetData (list, ikey, lidx);
}

void
ilistDelete (list_t *list, ilistidx_t ikey)
{
  ilistidx_t      idx;
  listkeylookup_t key;

  if (list == NULL) {
    return;
  }

  key.idx = ikey;
  idx = listGetIdx (list, &key);
  if (idx >= 0) {
    listDeleteByIdx (list, idx);
  }
}

inline void
ilistSort (ilist_t *list)
{
  listSort (list);
}

inline void
ilistStartIterator (ilist_t *list, ilistidx_t *iteridx)
{
  *iteridx = -1;
}

inline ilistidx_t
ilistIterateKey (ilist_t *list, ilistidx_t *iteridx)
{
  return listIterateKeyNum (list, iteridx);
}

void
ilistDumpInfo (ilist_t *list)
{
  listDumpInfo (list);
}

/* internal routines */

static void
ilistSetDataValType (ilist_t *list, ilistidx_t ikey, ilistidx_t lidx,
    void *data, valuetype_t valueType)
{
  listitem_t    item;

  if (list == NULL) {
    return;
  }

  item.key.idx = lidx;
  item.valuetype = valueType;
  item.value.data = data;
  ilistSetDataItem (list, ikey, lidx, &item);
}

static void
ilistSetDataItem (ilist_t *list, ilistidx_t ikey, ilistidx_t lidx,
    listitem_t *item)
{
  nlist_t       *datalist = NULL;
  char          tbuff [40];

  if (list == NULL) {
    return;
  }

  datalist = nlistGetList (list, ikey);
  if (datalist == NULL) {
    snprintf (tbuff, sizeof (tbuff), "%s-item-%d", list->name, ikey);
    datalist = nlistAlloc (tbuff, LIST_ORDERED, NULL);
    nlistSetList (list, ikey, datalist);
  }
  if (datalist != NULL) {
    listSet (datalist, item);
  }
  return;
}


