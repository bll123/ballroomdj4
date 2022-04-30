#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "bdjvarsdf.h"
#include "dance.h"
#include "datafile.h"
#include "log.h"
#include "uiutils.h"

datafilekey_t filterdisplaydfkeys [FILTER_DISP_MAX] = {
  { "DANCELEVEL",     FILTER_DISP_DANCELEVEL,      VALUE_NUM, convBoolean, -1 },
  { "FAVORITE",       FILTER_DISP_FAVORITE,        VALUE_NUM, convBoolean, -1 },
  { "GENRE",          FILTER_DISP_GENRE,           VALUE_NUM, convBoolean, -1 },
  { "STATUS",         FILTER_DISP_STATUS,          VALUE_NUM, convBoolean, -1 },
  { "STATUSPLAYABLE", FILTER_DISP_STATUSPLAYABLE,  VALUE_NUM, convBoolean, -1 },
};

void
uiutilsCreateDanceList (uiutilsdropdown_t *dropdown, char *selectLabel)
{
  dance_t           *dances;
  slist_t           *danceList;

  logProcBegin (LOG_PROC, "uiutilsCreateDanceList");

  dances = bdjvarsdfGet (BDJVDF_DANCES);
  danceList = danceGetDanceList (dances);

  uiutilsDropDownSetNumList (dropdown, danceList, selectLabel);
  logProcEnd (LOG_PROC, "uiutilsCreateDanceList", "");
}

uiutilsnbtabid_t *
uiutilsNotebookIDInit (void)
{
  uiutilsnbtabid_t *nbtabid;

  nbtabid = malloc (sizeof (uiutilsnbtabid_t));
  nbtabid->tabcount = 0;
  nbtabid->tabids = NULL;
  return nbtabid;
}

void
uiutilsNotebookIDFree (uiutilsnbtabid_t *nbtabid)
{
  if (nbtabid != NULL) {
    if (nbtabid->tabids != NULL) {
      free (nbtabid->tabids);
    }
    free (nbtabid);
  }
}

void
uiutilsNotebookIDAdd (uiutilsnbtabid_t *nbtabid, int id)
{
  nbtabid->tabids = realloc (nbtabid->tabids,
      sizeof (int) * (nbtabid->tabcount + 1));
  nbtabid->tabids [nbtabid->tabcount] = id;
  ++nbtabid->tabcount;
}

int
uiutilsNotebookIDGet (uiutilsnbtabid_t *nbtabid, int idx)
{
  if (nbtabid == NULL) {
    return 0;
  }
  if (idx >= nbtabid->tabcount) {
    return 0;
  }
  return nbtabid->tabids [idx];
}
