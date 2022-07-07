#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include "bdj4intl.h"
#include "bdjopt.h"
#include "bdjvarsdf.h"
#include "manageui.h"
#include "musicdb.h"
#include "musicq.h"
#include "nlist.h"
#include "song.h"
#include "tagdef.h"
#include "tmutil.h"
#include "ui.h"

enum {
  STATS_COLS = 5,
  STATS_PER_COL = 9,
  STATS_MAX_DISP = (STATS_COLS * STATS_PER_COL) * 2,
};

typedef struct managestats {
  conn_t      *conn;
  musicdb_t   *musicdb;
  UIWidget    vboxmain;
  UIWidget    dancedisp [STATS_MAX_DISP];
  nlist_t     *dancecounts;
  UIWidget    songcountdisp;
  UIWidget    tottimedisp;
  int         songcount;
  long        tottime;
} managestats_t;

static void manageStatsProcessData (managestats_t *managestats, char *data);
static void manageStatsDisplayStats (managestats_t *managestats);


managestats_t *
manageStatsInit (conn_t *conn, musicdb_t *musicdb)
{
  managestats_t *managestats;

  managestats = malloc (sizeof (managestats_t));
  managestats->conn = conn;
  managestats->musicdb = musicdb;
  uiutilsUIWidgetInit (&managestats->vboxmain);
  uiutilsUIWidgetInit (&managestats->songcountdisp);
  uiutilsUIWidgetInit (&managestats->tottimedisp);
  managestats->songcount = 0;
  managestats->tottime = 0;
  managestats->dancecounts = NULL;
  for (int i = 0; i < STATS_MAX_DISP; ++i) {
    uiutilsUIWidgetInit (&managestats->dancedisp [i]);
  }

  return managestats;
}

void
manageStatsFree (managestats_t *managestats)
{
  if (managestats != NULL) {
    if (managestats->dancecounts != NULL) {
      nlistFree (managestats->dancecounts);
    }
    free (managestats);
  }
}

UIWidget *
manageBuildUIStats (managestats_t *managestats)
{
  UIWidget    uiwidget;
  UIWidget    hbox;
  UIWidget    chbox;
  const char  *listingFont;

  uiCreateVertBox (&managestats->vboxmain);

  /* Number of songs */
  uiCreateHorizBox (&hbox);
  uiWidgetSetMarginTop (&hbox, uiBaseMarginSz * 2);
  uiBoxPackStart (&managestats->vboxmain, &hbox);

  uiCreateColonLabel (&uiwidget, _("Songs"));
  uiBoxPackStart (&hbox, &uiwidget);

  uiCreateLabel (&managestats->songcountdisp, "");
  uiLabelAlignEnd (&managestats->songcountdisp);
  uiBoxPackStart (&hbox, &managestats->songcountdisp);

  /* total time (same horiz row) */
  uiCreateColonLabel (&uiwidget, _("Total Time"));
  uiWidgetSetMarginStart (&uiwidget, uiBaseMarginSz * 10);
  uiBoxPackStart (&hbox, &uiwidget);

  uiCreateLabel (&managestats->tottimedisp, "");
  uiLabelAlignEnd (&managestats->tottimedisp);
  uiBoxPackStart (&hbox, &managestats->tottimedisp);

  listingFont = bdjoptGetStr (OPT_MP_LISTING_FONT);

  for (int i = 0; i < STATS_MAX_DISP; ++i) {
    uiCreateLabel (&managestats->dancedisp [i], "");
    if (i % 2 == 1) {
      uiWidgetSetSizeRequest (&managestats->dancedisp [i], 30, -1);
      uiLabelAlignEnd (&managestats->dancedisp [i]);
    }
    uiLabelSetFont (&managestats->dancedisp [i], listingFont);
  }

  /* horizontal box to hold the columns */
  uiCreateHorizBox (&chbox);
  uiWidgetSetMarginTop (&chbox, uiBaseMarginSz * 2);
  uiBoxPackStart (&managestats->vboxmain, &chbox);

  for (int i = 0; i < STATS_COLS; ++i) {
    UIWidget    vbox;

    /* vertical box for each column */
    uiCreateVertBox (&vbox);
    uiWidgetSetMarginTop (&vbox, uiBaseMarginSz * 2);
    uiWidgetSetMarginStart (&vbox, uiBaseMarginSz * 2);
    uiWidgetSetMarginEnd (&vbox, uiBaseMarginSz * 8);
    uiBoxPackStart (&chbox, &vbox);

    for (int j = 0; j < STATS_PER_COL; ++j) {
      int   idx;

      idx = i * STATS_PER_COL * 2 + j * 2;
      uiCreateHorizBox (&hbox);
      uiBoxPackStart (&vbox, &hbox);
      uiBoxPackStart (&hbox, &managestats->dancedisp [idx]);
      uiBoxPackEnd (&hbox, &managestats->dancedisp [idx + 1]);
    }
  }

  return &managestats->vboxmain;
}

void
manageStatsProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
    bdjmsgmsg_t msg, char *args, managestats_t *managestats)
{
  char          *targs = NULL;

  if (args != NULL) {
    targs = strdup (args);
  }

  switch (route) {
    case ROUTE_NONE:
    case ROUTE_MANAGEUI:
    case ROUTE_PLAYERUI: {
      switch (msg) {
        case MSG_MUSIC_QUEUE_DATA: {
          manageStatsProcessData (managestats, targs);
          // dbgdisp = true;
          break;
        }
        default: {
          break;
        }
      }
    }
    default: {
      break;
    }
  }

  if (targs != NULL) {
    free (targs);
  }
}

static void
manageStatsProcessData (managestats_t *managestats, char *data)
{
  int               ci;
  char              *p;
  char              *tokstr;
  dbidx_t           dbidx;
  ilistidx_t        danceIdx;
  song_t            *song;
  nlist_t           *dcounts;

  managestats->songcount = 0;
  managestats->tottime = 0;

  p = strtok_r (data, MSG_ARGS_RS_STR, &tokstr);
  ci = atoi (p);
  if (ci != MUSICQ_SL) {
    return;
  }

  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  managestats->tottime = atol (p);

  dcounts = nlistAlloc ("stats", LIST_ORDERED, NULL);

  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  while (p != NULL) {
    /* dispidx */
    p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
    /* uniqueidx */
    p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
    /* database idx */
    dbidx = atol (p);
    p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
    /* pause flag */

    song = dbGetByIdx (managestats->musicdb, dbidx);
    danceIdx = songGetNum (song, TAG_DANCE);
    nlistIncrement (dcounts, danceIdx);
    ++managestats->songcount;

    p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  }

  if (managestats->dancecounts != NULL) {
    nlistFree (managestats->dancecounts);
  }
  managestats->dancecounts = dcounts;

  /* this only needs to be called when the tab is displayed */
  manageStatsDisplayStats (managestats);
}

static void
manageStatsDisplayStats (managestats_t *managestats)
{
  char        tbuff [60];
  int         count;
  slist_t     *danceList;
  char        *dancedisp;
  dance_t     *dances;
  slistidx_t  diteridx;
  ilistidx_t  didx;
  int         val;


  tmutilToMS (managestats->tottime, tbuff, sizeof (tbuff));
  uiLabelSetText (&managestats->tottimedisp, tbuff);

  snprintf (tbuff, sizeof (tbuff), "%d", managestats->songcount);
  uiLabelSetText (&managestats->songcountdisp, tbuff);

  for (int i = 0; i < STATS_MAX_DISP; ++i) {
    uiLabelSetText (&managestats->dancedisp [i], "");
  }

  dances = bdjvarsdfGet (BDJVDF_DANCES);
  danceList = danceGetDanceList (dances);
  slistStartIterator (danceList, &diteridx);
  count = 0;
  while ((dancedisp = slistIterateKey (danceList, &diteridx)) != NULL) {
    didx = slistGetNum (danceList, dancedisp);
    val = nlistGetNum (managestats->dancecounts, didx);
    if (val > 0) {
      uiLabelSetText (&managestats->dancedisp [count], dancedisp);
      ++count;
      snprintf (tbuff, sizeof (tbuff), "%d", val);
      uiLabelSetText (&managestats->dancedisp [count], tbuff);
      ++count;
    }
  }
}

