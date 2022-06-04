#include "config.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>

#include "bdj4.h"
#include "bdj4intl.h"
#include "bdj4ui.h"
#include "bdjstring.h"
#include "bdjvarsdf.h"
#include "genre.h"
#include "ilist.h"
#include "level.h"
#include "log.h"
#include "musicq.h"
#include "nlist.h"
#include "rating.h"
#include "songfilter.h"
#include "sortopt.h"
#include "status.h"
#include "ui.h"
#include "uilevel.h"
#include "uirating.h"
#include "uisongfilter.h"
#include "uiutils.h"

typedef struct uisongfilter {
  rating_t          *ratings;
  level_t           *levels;
  status_t          *status;
  sortopt_t         *sortopt;
  UIWidget          *parentwin;
  nlist_t           *options;
  UICallback        *applycb;
  UICallback        *danceselcb;
  songfilter_t      *songfilter;
  UIWidget          filterDialog;
  UICallback        filtercb;
  uidropdown_t      sortbysel;
  uidropdown_t      filterdancesel;
  uidropdown_t      filtergenresel;
  uientry_t         *searchentry;
  uirating_t        *uirating;
  uilevel_t         *uilevel;
  uispinbox_t       *filterstatussel;
  uispinbox_t       *filterfavoritesel;
  uiswitch_t        *playstatusswitch;
  songfilterpb_t    dfltpbflag;
  time_t            filterApplied;
  int               danceIdx;
} uisongfilter_t;

/* song filter handling */
static void uisfCreateDialog (uisongfilter_t *uisf);
static bool uisfResponseHandler (void *udata, long responseid);
static void uisfUpdate (uisongfilter_t *uisf);
static void uisfSortBySelectHandler (GtkTreeView *tv, GtkTreePath *path,
    GtkTreeViewColumn *column, gpointer udata);
static void uisfGenreSelectHandler (GtkTreeView *tv, GtkTreePath *path,
    GtkTreeViewColumn *column, gpointer udata);
static void uisfDanceSelectHandler (GtkTreeView *tv, GtkTreePath *path,
    GtkTreeViewColumn *column, gpointer udata);
static void uisfInitDisplay (uisongfilter_t *uisf);
#if 0
static char *uisfRatingGet (void *udata, int idx);
static char *uisfLevelGet (void *udata, int idx);
#endif
static char *uisfStatusGet (void *udata, int idx);
static char *uisfFavoriteGet (void *udata, int idx);
static void uisfSetFavoriteForeground (uisongfilter_t *uisf, char *color);
static void uisfSortBySelect (uisongfilter_t *uisf, ssize_t idx);
static void uisfCreateSortByList (uisongfilter_t *uisf);
static void uisfGenreSelect (uisongfilter_t *uisf, ssize_t idx);
static void uisfCreateGenreList (uisongfilter_t *uisf);


uisongfilter_t *
uisfInit (UIWidget *windowp, nlist_t *options, songfilterpb_t pbflag)
{
  uisongfilter_t *uisf;

  uisf = malloc (sizeof (uisongfilter_t));

  uisf->ratings = bdjvarsdfGet (BDJVDF_RATINGS);
  uisf->levels = bdjvarsdfGet (BDJVDF_LEVELS);
  uisf->status = bdjvarsdfGet (BDJVDF_STATUS);
  uisf->sortopt = sortoptAlloc ();
  uisf->applycb = NULL;
  uisf->danceselcb = NULL;
  uisf->parentwin = windowp;
  uisf->options = options;
  uisf->songfilter = songfilterAlloc ();
  songfilterSetSort (uisf->songfilter,
      nlistGetStr (options, SONGSEL_SORT_BY));
  uiutilsUIWidgetInit (&uisf->filterDialog);
  uisf->dfltpbflag = pbflag;
  uiDropDownInit (&uisf->sortbysel);
  uiDropDownInit (&uisf->filtergenresel);
  uiDropDownInit (&uisf->filterdancesel);
  uisf->filterstatussel = uiSpinboxTextInit ();
  uisf->filterfavoritesel = uiSpinboxTextInit ();
  uisf->searchentry = uiEntryInit (30, 100);
  uisf->uirating = NULL;
  uisf->uilevel = NULL;
  uisf->filterApplied = mstime ();
  uisf->playstatusswitch = NULL;
  uisf->danceIdx = -1;

  return uisf;
}

void
uisfFree (uisongfilter_t *uisf)
{
  if (uisf != NULL) {
    uiDialogDestroy (&uisf->filterDialog);
    uiDropDownFree (&uisf->sortbysel);
    uiEntryFree (uisf->searchentry);
    uiDropDownFree (&uisf->filterdancesel);
    uiDropDownFree (&uisf->filtergenresel);
    uiratingFree (uisf->uirating);
    uilevelFree (uisf->uilevel);
    uiSpinboxTextFree (uisf->filterstatussel);
    uiSpinboxTextFree (uisf->filterfavoritesel);
    uiSwitchFree (uisf->playstatusswitch);
    sortoptFree (uisf->sortopt);
    free (uisf);
  }
}

void
uisfSetApplyCallback (uisongfilter_t *uisf, UICallback *applycb)
{
  if (uisf == NULL) {
    return;
  }
  uisf->applycb = applycb;
}

void
uisfSetDanceSelectCallback (uisongfilter_t *uisf, UICallback *danceselcb)
{
  if (uisf == NULL) {
    return;
  }
  uisf->danceselcb = danceselcb;
}

bool
uisfDialog (void *udata)
{
  uisongfilter_t *uisf = udata;
  int         x, y;

  logProcBegin (LOG_PROC, "uisfDialog");

  uisfCreateDialog (uisf);
  uisfInitDisplay (uisf);
  if (songfilterCheckSelection (uisf->songfilter, FILTER_DISP_STATUSPLAYABLE)) {
    uiSwitchSetValue (uisf->playstatusswitch, uisf->dfltpbflag);
  }
  uiWidgetShowAll (&uisf->filterDialog);

  x = nlistGetNum (uisf->options, SONGSEL_FILTER_POSITION_X);
  y = nlistGetNum (uisf->options, SONGSEL_FILTER_POSITION_Y);
  uiWindowMove (&uisf->filterDialog, x, y);
  logProcEnd (LOG_PROC, "uisfDialog", "");
  return UICB_CONT;
}

void
uisfSetDanceIdx (uisongfilter_t *uisf, int danceIdx)
{
  if (uisf == NULL) {
    return;
  }

  uisf->danceIdx = danceIdx;
  uiDropDownSelectionSetNum (&uisf->filterdancesel, danceIdx);

  if (danceIdx >= 0) {
    ilist_t   *danceList;

    danceList = ilistAlloc ("songfilter-dance", LIST_ORDERED);
    /* any value will do; only interested in the dance index at this point */
    ilistSetNum (danceList, danceIdx, 0, 0);
    songfilterSetData (uisf->songfilter, SONG_FILTER_DANCE, danceList);
    songfilterSetNum (uisf->songfilter, SONG_FILTER_DANCE_IDX, danceIdx);
  } else {
    songfilterClear (uisf->songfilter, SONG_FILTER_DANCE);
    songfilterClear (uisf->songfilter, SONG_FILTER_DANCE_IDX);
  }
}

songfilter_t *
uisfGetSongFilter (uisongfilter_t *uisf)
{
  if (uisf == NULL) {
    return NULL;
  }
  return uisf->songfilter;
}

/* internal routines */

static void
uisfInitDisplay (uisongfilter_t *uisf)
{
  char        *sortby;

  logProcBegin (LOG_PROC, "uisfInitFilterDisplay");

  /* this is run when the filter dialog is first started, */
  /* and after a reset */
  /* all items need to be set, as after a reset, they need to be updated */
  /* sort-by and dance are important, the others can be reset */

  sortby = songfilterGetSort (uisf->songfilter);
  uiDropDownSelectionSetStr (&uisf->sortbysel, sortby);

  uiDropDownSelectionSetNum (&uisf->filterdancesel, uisf->danceIdx);

  uiDropDownSelectionSetNum (&uisf->filtergenresel, -1);
  uiEntrySetValue (uisf->searchentry, "");
  uiratingSetValue (uisf->uirating, -1);
  uilevelSetValue (uisf->uilevel, -1);
  uiSpinboxTextSetValue (uisf->filterstatussel, -1);
  uiSpinboxTextSetValue (uisf->filterfavoritesel, 0);
  logProcEnd (LOG_PROC, "uisfInitFilterDisplay", "");
}

#if 0

char *
uisfRatingGet (void *udata, int idx)
{
  uisongfilter_t *uisf = udata;

  logProcBegin (LOG_PROC, "uisfRatingGet");

  if (idx == -1) {
    logProcEnd (LOG_PROC, "uisfRatingGet", "all");
    /* CONTEXT: a filter: all dance ratings are displayed in the song selection */
    return _("All Ratings");
  }
  logProcEnd (LOG_PROC, "uisfRatingGet", "");
  return ratingGetRating (uisf->ratings, idx);
}

char *
uisfLevelGet (void *udata, int idx)
{
  uisongfilter_t *uisf = udata;

  logProcBegin (LOG_PROC, "uisfLevelGet");

  if (idx == -1) {
    logProcEnd (LOG_PROC, "uisfLevelGet", "all");
    /* CONTEXT: a filter: all dance levels are displayed in the song selection */
    return _("All Levels");
  }
  logProcEnd (LOG_PROC, "uisfLevelGet", "");
  return levelGetLevel (uisf->levels, idx);
}

#endif

char *
uisfStatusGet (void *udata, int idx)
{
  uisongfilter_t *uisf = udata;

  logProcBegin (LOG_PROC, "uisfStatusGet");

  if (idx == -1) {
    logProcEnd (LOG_PROC, "uisfStatusGet", "any");
    /* CONTEXT: a filter: all statuses are displayed in the song selection */
    return _("Any Status");
  }
  logProcEnd (LOG_PROC, "uisfStatusGet", "");
  return statusGetStatus (uisf->status, idx);
}

static char *
uisfFavoriteGet (void *udata, int idx)
{
  uisongfilter_t      *uisf = udata;
  songfavoriteinfo_t  *favorite;

  logProcBegin (LOG_PROC, "uisfFavoriteGet");

  favorite = songGetFavorite (idx);
  uisfSetFavoriteForeground (uisf, favorite->color);
  logProcEnd (LOG_PROC, "uisfFavoriteGet", "");
  return favorite->dispStr;
}


static void
uisfSetFavoriteForeground (uisongfilter_t *uisf, char *color)
{
  char                tmp [40];

  if (strcmp (color, "") != 0) {
    uiSpinboxSetColor (uisf->filterfavoritesel, color);
  } else {
    uiGetForegroundColor (&uisf->filterDialog, tmp, sizeof (tmp));
    uiSpinboxSetColor (uisf->filterfavoritesel, tmp);
  }
}

static void
uisfSortBySelect (uisongfilter_t *uisf, ssize_t idx)
{
  logProcBegin (LOG_PROC, "uisfSortBySelect");
  if (idx >= 0) {
    songfilterSetSort (uisf->songfilter,
        uisf->sortbysel.strSelection);
    nlistSetStr (uisf->options, SONGSEL_SORT_BY,
        uisf->sortbysel.strSelection);
  }
  logProcEnd (LOG_PROC, "uisfSortBySelect", "");
}

static void
uisfCreateSortByList (uisongfilter_t *uisf)
{
  slist_t           *sortoptlist;

  logProcBegin (LOG_PROC, "uisfCreateSortByList");

  sortoptlist = sortoptGetList (uisf->sortopt);
  uiDropDownSetList (&uisf->sortbysel, sortoptlist, NULL);
  logProcEnd (LOG_PROC, "uisfCreateSortByList", "");
}

static void
uisfGenreSelect (uisongfilter_t *uisf, ssize_t idx)
{
  logProcBegin (LOG_PROC, "uisfGenreSelect");
  if (songfilterCheckSelection (uisf->songfilter, FILTER_DISP_GENRE)) {
    if (idx >= 0) {
      songfilterSetNum (uisf->songfilter, SONG_FILTER_GENRE, idx);
    } else {
      songfilterClear (uisf->songfilter, SONG_FILTER_GENRE);
    }
  }
  logProcEnd (LOG_PROC, "uisfGenreSelect", "");
}

static void
uisfCreateGenreList (uisongfilter_t *uisf)
{
  slist_t   *genrelist;
  genre_t   *genre;

  logProcBegin (LOG_PROC, "uisfCreateGenreList");

  genre = bdjvarsdfGet (BDJVDF_GENRES);
  genrelist = genreGetList (genre);
  uiDropDownSetNumList (&uisf->filtergenresel, genrelist,
      /* CONTEXT: a filter: all genres are displayed in the song selection */
      _("All Genres"));
  logProcEnd (LOG_PROC, "uisfCreateGenreList", "");
}

static void
uisfCreateDialog (uisongfilter_t *uisf)
{
  GtkWidget     *widget;
  UIWidget      vbox;
  UIWidget      hbox;
  UIWidget      uiwidget;
  UIWidget      *uiwidgetp;
  UIWidget      sg;
  int           max;
  int           len;

  logProcBegin (LOG_PROC, "uisfCreateFilterDialog");

  uiCreateSizeGroupHoriz (&sg);

  uiutilsUICallbackLongInit (&uisf->filtercb,
      uisfResponseHandler, uisf);
  uiCreateDialog (&uisf->filterDialog, uisf->parentwin,
      &uisf->filtercb,
      /* CONTEXT: title for the filter dialog */
      _("Filter Songs"),
      /* CONTEXT: filter dialog: closes the dialog */
      _("Close"),
      RESPONSE_CLOSE,
      /* CONTEXT: filter dialog: resets the selections */
      _("Reset"),
      RESPONSE_RESET,
      /* CONTEXT: filter dialog: applies the selections */
      _("Apply"),
      RESPONSE_APPLY,
      NULL
      );

  uiCreateVertBox (&vbox);
  uiDialogPackInDialog (&uisf->filterDialog, &vbox);

  /* sort-by : always available */
  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&vbox, &hbox);

  /* CONTEXT: a filter: select the method to sort the song selection display */
  uiCreateColonLabel (&uiwidget, _("Sort by"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  widget = uiComboboxCreate (uisf->filterDialog.widget,
      "", uisfSortBySelectHandler, &uisf->sortbysel, uisf);
  uisfCreateSortByList (uisf);
  uiBoxPackStartUW (&hbox, widget);

  /* search : always available */
  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&vbox, &hbox);

  /* CONTEXT: a filter: filter the song selection with a search for text */
  uiCreateColonLabel (&uiwidget, _("Search"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  uiEntryCreate (uisf->searchentry);
  uiwidgetp = uiEntryGetUIWidget (uisf->searchentry);
  uiWidgetAlignHorizStart (uiwidgetp);
  uiBoxPackStart (&hbox, uiwidgetp);

  /* genre */
  if (songfilterCheckSelection (uisf->songfilter, FILTER_DISP_GENRE)) {
    uiCreateHorizBox (&hbox);
    uiBoxPackStart (&vbox, &hbox);

    /* CONTEXT: a filter: select the genre displayed in the song selection */
    uiCreateColonLabel (&uiwidget, _("Genre"));
    uiBoxPackStart (&hbox, &uiwidget);
    uiSizeGroupAdd (&sg, &uiwidget);

    widget = uiComboboxCreate (uisf->filterDialog.widget,
        "", uisfGenreSelectHandler,
        &uisf->filtergenresel, uisf);
    uisfCreateGenreList (uisf);
    uiBoxPackStartUW (&hbox, widget);
  }

  /* dance : always available */
  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&vbox, &hbox);

  /* CONTEXT: a filter: select the dance displayed in the song selection */
  uiCreateColonLabel (&uiwidget, _("Dance"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  widget = uiComboboxCreate (uisf->filterDialog.widget,
      "", uisfDanceSelectHandler,
      &uisf->filterdancesel, uisf);
  /* CONTEXT: a filter: all dances are selected */
  uiutilsCreateDanceList (&uisf->filterdancesel, _("All Dances"));
  uiBoxPackStartUW (&hbox, widget);

  /* rating : always available */
  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&vbox, &hbox);

  /* CONTEXT: a filter: select the dance rating displayed in the song selection */
  uiCreateColonLabel (&uiwidget, _("Dance Rating"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  uisf->uirating = uiratingSpinboxCreate (&hbox, true);

  /* level */
  if (songfilterCheckSelection (uisf->songfilter, FILTER_DISP_DANCELEVEL)) {
    uiCreateHorizBox (&hbox);
    uiBoxPackStart (&vbox, &hbox);

    /* CONTEXT: a filter: select the dance level displayed in the song selection */
    uiCreateColonLabel (&uiwidget, _("Dance Level"));
    uiBoxPackStart (&hbox, &uiwidget);
    uiSizeGroupAdd (&sg, &uiwidget);

    uisf->uilevel = uilevelSpinboxCreate (&hbox, true);
  }

  /* status */
  if (songfilterCheckSelection (uisf->songfilter, FILTER_DISP_STATUS)) {
    uiCreateHorizBox (&hbox);
    uiBoxPackStart (&vbox, &hbox);

    /* CONTEXT: a filter: select the status displayed in the song selection */
    uiCreateColonLabel (&uiwidget, _("Status"));
    uiBoxPackStart (&hbox, &uiwidget);
    uiSizeGroupAdd (&sg, &uiwidget);

    uiSpinboxTextCreate (uisf->filterstatussel, uisf);
    max = statusGetMaxWidth (uisf->status);
    /* CONTEXT: a filter: all statuses are displayed in the song selection */
    len = istrlen (_("Any Status"));
    if (len > max) {
      max = len;
    }
    uiSpinboxTextSet (uisf->filterstatussel, -1,
        statusGetCount (uisf->status),
        max, NULL, NULL, uisfStatusGet);
    uiBoxPackStart (&hbox, uiSpinboxGetUIWidget (uisf->filterstatussel));
  }

  /* favorite */
  if (songfilterCheckSelection (uisf->songfilter, FILTER_DISP_FAVORITE)) {
    uiCreateHorizBox (&hbox);
    uiBoxPackStart (&vbox, &hbox);

    /* CONTEXT: a filter: select the 'favorite' displayed in the song selection */
    uiCreateColonLabel (&uiwidget, _("Favorite"));
    uiBoxPackStart (&hbox, &uiwidget);
    uiSizeGroupAdd (&sg, &uiwidget);

    uiSpinboxTextCreate (uisf->filterfavoritesel, uisf);
    uiSpinboxTextSet (uisf->filterfavoritesel, 0,
        SONG_FAVORITE_MAX, 1, NULL, NULL, uisfFavoriteGet);
    uiBoxPackStart (&hbox, uiSpinboxGetUIWidget (uisf->filterfavoritesel));
  }

  /* status playable */
  if (songfilterCheckSelection (uisf->songfilter, FILTER_DISP_STATUSPLAYABLE)) {
    uiCreateHorizBox (&hbox);
    uiBoxPackStart (&vbox, &hbox);

    /* CONTEXT: a filter: the song status is marked as playable */
    uiCreateColonLabel (&uiwidget, _("Playable Status"));
    uiBoxPackStart (&hbox, &uiwidget);
    uiSizeGroupAdd (&sg, &uiwidget);

    uisf->playstatusswitch = uiCreateSwitch (uisf->dfltpbflag);
    uiBoxPackStart (&hbox, uiSwitchGetUIWidget (uisf->playstatusswitch));
  }

  /* the dialog doesn't have any space above the buttons */
  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&vbox, &hbox);

  uiCreateLabel (&uiwidget, " ");
  uiBoxPackStart (&hbox, &uiwidget);

  logProcEnd (LOG_PROC, "uisfCreateFilterDialog", "");
}

static bool
uisfResponseHandler (void *udata, long responseid)
{
  uisongfilter_t   *uisf = udata;
  int           x, y;

  logProcBegin (LOG_PROC, "uisfResponseHandler");

  switch (responseid) {
    case RESPONSE_DELETE_WIN: {
      uiWindowGetPosition (&uisf->filterDialog, &x, &y);
      nlistSetNum (uisf->options, SONGSEL_FILTER_POSITION_X, x);
      nlistSetNum (uisf->options, SONGSEL_FILTER_POSITION_Y, y);
      uiutilsUIWidgetInit (&uisf->filterDialog);
      break;
    }
    case RESPONSE_CLOSE: {
      uiWindowGetPosition (&uisf->filterDialog, &x, &y);
      nlistSetNum (uisf->options, SONGSEL_FILTER_POSITION_X, x);
      nlistSetNum (uisf->options, SONGSEL_FILTER_POSITION_Y, y);
      uiWidgetHide (&uisf->filterDialog);
      break;
    }
    case RESPONSE_APPLY: {
      break;
    }
    case RESPONSE_RESET: {
      songfilterReset (uisf->songfilter);
      uisf->danceIdx = -1;
      uiDropDownSelectionSetNum (&uisf->filterdancesel, uisf->danceIdx);
      if (uisf->danceselcb != NULL) {
        uiutilsCallbackLongHandler (uisf->danceselcb, uisf->danceIdx);
      }
      uisfInitDisplay (uisf);
      if (songfilterCheckSelection (uisf->songfilter, FILTER_DISP_STATUSPLAYABLE)) {
        uiSwitchSetValue (uisf->playstatusswitch, uisf->dfltpbflag);
      }
      break;
    }
  }

  if (responseid != RESPONSE_DELETE_WIN) {
    uisfUpdate (uisf);
  }
  return UICB_CONT;
}

void
uisfUpdate (uisongfilter_t *uisf)
{
  const char    *searchstr;
  int           idx;
  int           nval;


  /* search : always active */
  searchstr = uiEntryGetValue (uisf->searchentry);
  if (searchstr != NULL && strlen (searchstr) > 0) {
    songfilterSetData (uisf->songfilter, SONG_FILTER_SEARCH, (void *) searchstr);
  } else {
    songfilterClear (uisf->songfilter, SONG_FILTER_SEARCH);
  }

  /* dance rating : always active */
  idx = uiratingGetValue (uisf->uirating);
  if (idx >= 0) {
    songfilterSetNum (uisf->songfilter, SONG_FILTER_RATING, idx);
  } else {
    songfilterClear (uisf->songfilter, SONG_FILTER_RATING);
  }

  /* dance level */
  if (songfilterCheckSelection (uisf->songfilter, FILTER_DISP_DANCELEVEL)) {
    idx = uilevelGetValue (uisf->uilevel);
    if (idx >= 0) {
      songfilterSetNum (uisf->songfilter, SONG_FILTER_LEVEL_LOW, idx);
      songfilterSetNum (uisf->songfilter, SONG_FILTER_LEVEL_HIGH, idx);
    } else {
      songfilterClear (uisf->songfilter, SONG_FILTER_LEVEL_LOW);
      songfilterClear (uisf->songfilter, SONG_FILTER_LEVEL_HIGH);
    }
  }

  /* status */
  if (songfilterCheckSelection (uisf->songfilter, FILTER_DISP_STATUS)) {
    idx = uiSpinboxTextGetValue (uisf->filterstatussel);
    if (idx >= 0) {
      songfilterSetNum (uisf->songfilter, SONG_FILTER_STATUS, idx);
    } else {
      songfilterClear (uisf->songfilter, SONG_FILTER_STATUS);
    }
  }

  /* favorite */
  if (songfilterCheckSelection (uisf->songfilter, FILTER_DISP_FAVORITE)) {
    idx = uiSpinboxTextGetValue (uisf->filterfavoritesel);
    if (idx != SONG_FAVORITE_NONE) {
      songfilterSetNum (uisf->songfilter, SONG_FILTER_FAVORITE, idx);
    } else {
      songfilterClear (uisf->songfilter, SONG_FILTER_FAVORITE);
    }
  }

  if (songfilterCheckSelection (uisf->songfilter, FILTER_DISP_STATUSPLAYABLE)) {
    nval = uiSwitchGetValue (uisf->playstatusswitch);
  } else {
    nval = uisf->dfltpbflag;
  }
  if (nval) {
    songfilterSetNum (uisf->songfilter, SONG_FILTER_STATUS_PLAYABLE, nval);
  } else {
    songfilterClear (uisf->songfilter, SONG_FILTER_STATUS_PLAYABLE);
  }

  if (uisf->applycb != NULL) {
    uiutilsCallbackHandler (uisf->applycb);
  }
  logProcEnd (LOG_PROC, "uisfResponseHandler", "");
}

static void
uisfSortBySelectHandler (GtkTreeView *tv, GtkTreePath *path,
    GtkTreeViewColumn *column, gpointer udata)
{
  uisongfilter_t *uisf = udata;
  ssize_t     idx;

  idx = uiDropDownSelectionGet (&uisf->sortbysel, path);
  uisfSortBySelect (uisf, idx);
}

static void
uisfGenreSelectHandler (GtkTreeView *tv, GtkTreePath *path,
    GtkTreeViewColumn *column, gpointer udata)
{
  uisongfilter_t *uisf = udata;
  ssize_t     idx;

  idx = uiDropDownSelectionGet (&uisf->filtergenresel, path);
  uisfGenreSelect (uisf, idx);
}

static void
uisfDanceSelectHandler (GtkTreeView *tv, GtkTreePath *path,
    GtkTreeViewColumn *column, gpointer udata)
{
  uisongfilter_t  *uisf = udata;
  long            danceIdx;

  danceIdx = uiDropDownSelectionGet (&uisf->filterdancesel, path);
  uisf->danceIdx = danceIdx;
  uisfSetDanceIdx (uisf, danceIdx);
  if (uisf->danceselcb != NULL) {
    uiutilsCallbackLongHandler (uisf->danceselcb, danceIdx);
  }
}
