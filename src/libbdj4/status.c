#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "bdj4.h"
#include "bdjstring.h"
#include "bdjvarsdf.h"
#include "datafile.h"
#include "fileop.h"
#include "ilist.h"
#include "log.h"
#include "pathbld.h"
#include "slist.h"
#include "status.h"

  /* must be sorted in ascii order */
static datafilekey_t statusdfkeys [STATUS_KEY_MAX] = {
  { "PLAYFLAG",   STATUS_PLAY_FLAG,   VALUE_NUM, convBoolean, -1 },
  { "STATUS",     STATUS_STATUS,      VALUE_STR, NULL, -1 },
};

status_t *
statusAlloc (void)
{
  status_t    *status;
  ilistidx_t  key;
  ilistidx_t  iteridx;
  char        fname [MAXPATHLEN];


  pathbldMakePath (fname, sizeof (fname), "", "status", ".txt", PATHBLD_MP_NONE);
  if (! fileopFileExists (fname)) {
    logMsg (LOG_DBG, LOG_IMPORTANT, "ERR: status: missing %s", fname);
    return NULL;
  }

  status = malloc (sizeof (status_t));
  assert (status != NULL);

  status->path = strdup (fname);
  status->df = datafileAllocParse ("status", DFTYPE_INDIRECT, fname,
      statusdfkeys, STATUS_KEY_MAX, STATUS_STATUS);
  status->status = datafileGetList (status->df);
  ilistDumpInfo (status->status);

  status->maxWidth = 0;
  ilistStartIterator (status->status, &iteridx);
  while ((key = ilistIterateKey (status->status, &iteridx)) >= 0) {
    char    *val;
    int     len;

    val = ilistGetStr (status->status, key, STATUS_STATUS);
    len = istrlen (val);
    if (len > status->maxWidth) {
      status->maxWidth = len;
    }
  }

  return status;
}

void
statusFree (status_t *status)
{
  if (status != NULL) {
    if (status->path != NULL) {
      free (status->path);
    }
    if (status->df != NULL) {
      datafileFree (status->df);
    }
    free (status);
  }
}

ssize_t
statusGetCount (status_t *status)
{
  return ilistGetCount (status->status);
}

int
statusGetMaxWidth (status_t *status)
{
  return status->maxWidth;
}

char *
statusGetStatus (status_t *status, ilistidx_t idx)
{
  return ilistGetStr (status->status, idx, STATUS_STATUS);
}

ssize_t
statusGetPlayFlag (status_t *status, ilistidx_t ikey)
{
  return ilistGetNum (status->status, ikey, STATUS_PLAY_FLAG);
}

void
statusStartIterator (status_t *status, ilistidx_t *iteridx)
{
  ilistStartIterator (status->status, iteridx);
}

ilistidx_t
statusIterate (status_t *status, ilistidx_t *iteridx)
{
  return ilistIterateKey (status->status, iteridx);
}

void
statusConv (datafileconv_t *conv)
{
  status_t      *status;
  slist_t       *lookup;
  ssize_t       num;

  status = bdjvarsdfGet (BDJVDF_STATUS);

  conv->allocated = false;
  if (conv->valuetype == VALUE_STR) {
    conv->valuetype = VALUE_NUM;

    if (status == NULL) {
      conv->u.num = 0;
      return;
    }

    lookup = datafileGetLookup (status->df);
    num = slistGetNum (lookup, conv->u.str);
    conv->u.num = num;
  } else if (conv->valuetype == VALUE_NUM) {
    conv->valuetype = VALUE_STR;

    if (status == NULL || conv->u.num == LIST_VALUE_INVALID) {
      conv->u.str = "New";
      return;
    }

    num = conv->u.num;
    conv->u.str = ilistGetStr (status->status, num, STATUS_STATUS);
  }
}

void
statusSave (status_t *status, ilist_t *list)
{
  datafileSaveIndirect ("status", status->path, statusdfkeys,
      STATUS_KEY_MAX, list);
}