#include "config.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "bdj4.h"
#include "bdj4intl.h"
#include "bdjopt.h"
#include "bdjstring.h"
#include "bdjvarsdf.h"
#include "conn.h"
#include "dance.h"
#include "dispsel.h"
#include "ilist.h"
#include "log.h"
#include "musicdb.h"
#include "musicq.h"
#include "nlist.h"
#include "pathbld.h"
#include "playlist.h"
#include "progstate.h"
#include "slist.h"
#include "song.h"
#include "tagdef.h"
#include "uimusicq.h"
#include "uiutils.h"

enum {
  MUSICQ_COL_ELLIPSIZE,
  MUSICQ_COL_FONT,
  MUSICQ_COL_IDX,
  MUSICQ_COL_UNIQUE_IDX,
  MUSICQ_COL_DBIDX,
  MUSICQ_COL_DISP_IDX,
  MUSICQ_COL_PAUSEIND,
  MUSICQ_COL_MAX,
  MUSICQ_COL_DANCE,
  MUSICQ_COL_TITLE,
  MUSICQ_COL_ARTIST,
};

enum {
  UIMUSICQ_SEL_NONE,
  UIMUSICQ_SEL_PREV,
  UIMUSICQ_SEL_NEXT,
  UIMUSICQ_SEL_TOP,
};

typedef struct {
  slistidx_t      dbidx;
  int             idx;
  int             dispidx;
  int             uniqueidx;
  int             pflag;
} musicqupdate_t;

static void   uimusicqCreatePlaylistList (uimusicq_t *uimusicq);
static void   uimusicqClearQueueProcess (GtkButton *b, gpointer udata);
static void   uimusicqQueueDanceProcess (GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *column, gpointer udata);
static void   uimusicqQueuePlaylistProcess (GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *column, gpointer udata);
static void   uimusicqProcessSongSelect (uimusicq_t *uimusicq, char * args);
static void   uimusicqProcessMusicQueueData (uimusicq_t *uimusicq, char * args);
static void   uimusicqProcessMusicQueueDataNew (uimusicq_t *uimusicq, char * args);
static void   uimusicqProcessMusicQueueDataUpdate (uimusicq_t *uimusicq, char * args);
static int    uimusicqMusicQueueDataFindRemovals (GtkTreeModel *model,
                GtkTreePath *path, GtkTreeIter *iter, gpointer udata);
static int    uimusicqMusicQueueDataParse (uimusicq_t *uimusicq, char * args);
static void   uimusicqMoveTopProcess (GtkButton *b, gpointer udata);
static void   uimusicqStopRepeat (GtkButton *b, gpointer udata);
static gboolean uimusicqMoveUpRepeat (void *udata);
static void   uimusicqMoveUpProcess (GtkButton *b, gpointer udata);
static gboolean uimusicqMoveDownRepeat (void *udata);
static void   uimusicqMoveDownProcess (GtkButton *b, gpointer udata);
static void   uimusicqTogglePauseProcess (GtkButton *b, gpointer udata);
static void   uimusicqRemoveProcess (GtkButton *b, gpointer udata);
static void   uimusicqSetSelection (uimusicq_t *uimusicq, char *pathstr);
static ssize_t uimusicqGetSelection (uimusicq_t *uimusicq);
static void   uimusicqMusicQueueSetSelected (uimusicq_t *uimusicq, int ci, int which);
static void   uimusicqSetMusicqDisplay (uimusicq_t *uimusicq,
    GtkListStore *store, GtkTreeIter *iter, song_t *song);

uimusicq_t *
uimusicqInit (progstate_t *progstate, conn_t *conn, dispsel_t *dispsel)
{
  uimusicq_t    *uimusicq;


  logProcBegin (LOG_PROC, "uimusicqInit");

  uimusicq = malloc (sizeof (uimusicq_t));
  assert (uimusicq != NULL);

  uimusicq->progstate = progstate;
  uimusicq->conn = conn;
  uimusicq->dispsel = dispsel;
  uimusicq->uniqueList = NULL;
  uimusicq->dispList = NULL;
  uimusicq->workList = NULL;
  for (int i = 0; i < MUSICQ_MAX; ++i) {
    uimusicq->ui [i].repeatTimer = 0;
    uiutilsDropDownInit (&uimusicq->ui [i].playlistsel);
    uiutilsDropDownInit (&uimusicq->ui [i].dancesel);
    uimusicq->ui [i].selPathStr = NULL;
    mstimeset (&uimusicq->ui [i].rowChangeTimer, 3600000);
  }
  uimusicq->musicqManageIdx = MUSICQ_A;
  uimusicq->musicqPlayIdx = MUSICQ_A;

  logProcEnd (LOG_PROC, "uimusicqInit", "");
  return uimusicq;
}

void
uimusicqFree (uimusicq_t *uimusicq)
{
  logProcBegin (LOG_PROC, "uimusicqFree");
  if (uimusicq != NULL) {
    for (int i = 0; i < MUSICQ_MAX; ++i) {
      if (uimusicq->ui [i].selPathStr != NULL) {
        free (uimusicq->ui [i].selPathStr);
      }
      uiutilsDropDownFree (&uimusicq->ui [i].playlistsel);
      uiutilsDropDownFree (&uimusicq->ui [i].dancesel);
    }
    free (uimusicq);
  }
  logProcEnd (LOG_PROC, "uimusicqFree", "");
}

GtkWidget *
uimusicqActivate (uimusicq_t *uimusicq, GtkWidget *parentwin, int ci)
{
  int                   tci;
  char                  tbuff [MAXPATHLEN];
  GtkWidget             *image = NULL;
  GtkWidget             *widget = NULL;
  GtkWidget             *hbox = NULL;
  GtkCellRenderer       *renderer = NULL;
  GtkTreeViewColumn     *column = NULL;
  slist_t               *sellist;


  logProcBegin (LOG_PROC, "uimusicqActivate");

  /* temporary */
  tci = uimusicq->musicqManageIdx;
  uimusicq->musicqManageIdx = ci;

  uimusicq->parentwin = parentwin;

  /* want a copy of the pixbuf for this image */
  pathbldMakePath (tbuff, sizeof (tbuff), "", "button_pause", ".svg",
      PATHBLD_MP_IMGDIR);
  image = gtk_image_new_from_file (tbuff);
  uimusicq->pauseImg = gtk_image_get_pixbuf (GTK_IMAGE (image));

  uimusicq->ui [ci].box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  assert (uimusicq->ui [ci].box != NULL);
  gtk_widget_set_hexpand (uimusicq->ui [ci].box, TRUE);
  gtk_widget_set_vexpand (uimusicq->ui [ci].box, TRUE);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  assert (hbox != NULL);
  gtk_widget_set_margin_top (hbox, 2);
  gtk_widget_set_hexpand (hbox, TRUE);
  gtk_box_pack_start (GTK_BOX (uimusicq->ui [ci].box), hbox,
      FALSE, FALSE, 0);

  /* CONTEXT: button: move the selected song to the top of the queue */
  widget = uiutilsCreateButton (_("Move to Top"), "button_movetop",
      uimusicqMoveTopProcess, uimusicq);
  gtk_box_pack_start (GTK_BOX (hbox), widget,
      FALSE, FALSE, 0);

  /* CONTEXT: button: move the selected song up in the queue */
  widget = uiutilsCreateButton (_("Move Up"), "button_up",
      NULL, uimusicq);
  g_signal_connect (widget, "pressed",
      G_CALLBACK (uimusicqMoveUpProcess), uimusicq);
  g_signal_connect (widget, "released",
      G_CALLBACK (uimusicqStopRepeat), uimusicq);
  gtk_box_pack_start (GTK_BOX (hbox), widget,
      FALSE, FALSE, 0);

  /* CONTEXT: button: move the selected song down in the queue */
  widget = uiutilsCreateButton (_("Move Down"), "button_down",
      NULL, uimusicq);
  g_signal_connect (widget, "pressed",
      G_CALLBACK (uimusicqMoveDownProcess), uimusicq);
  g_signal_connect (widget, "released",
      G_CALLBACK (uimusicqStopRepeat), uimusicq);
  gtk_box_pack_start (GTK_BOX (hbox), widget,
      FALSE, FALSE, 0);

  /* CONTEXT: button: set playback to pause after the selected song is played (toggle) */
  widget = uiutilsCreateButton (_("Toggle Pause"), "button_pause",
      uimusicqTogglePauseProcess, uimusicq);
  gtk_box_pack_start (GTK_BOX (hbox), widget,
      FALSE, FALSE, 0);

  /* CONTEXT: button: remove the song from the queue */
  widget = uiutilsCreateButton (_("Remove from queue"), "button_audioremove",
      uimusicqRemoveProcess, uimusicq);
  gtk_box_pack_start (GTK_BOX (hbox), widget,
      FALSE, FALSE, 0);

  // ### TODO create code to handle this.
  /* CONTEXT: button: request playback of a song external to BDJ4 (not in the database) */
  widget = uiutilsCreateButton (_("Request External"), NULL,
      NULL, uimusicq);
  gtk_box_pack_end (GTK_BOX (hbox), widget,
      FALSE, FALSE, 0);

  widget = uiutilsDropDownCreate (parentwin,
      /* CONTEXT: button: queue a playlist for playback */
      _("Queue Playlist"), uimusicqQueuePlaylistProcess,
      &uimusicq->ui [ci].playlistsel, uimusicq);
  gtk_box_pack_end (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  uimusicqCreatePlaylistList (uimusicq);

  widget = uiutilsDropDownCreate (parentwin,
      /* CONTEXT: button: queue a dance for playback */
      _("Queue Dance"), uimusicqQueueDanceProcess,
      &uimusicq->ui [ci].dancesel, uimusicq);
  uiutilsCreateDanceList (&uimusicq->ui [ci].dancesel, NULL);
  gtk_box_pack_end (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

  /* CONTEXT: button: clear the queue */
  widget = uiutilsCreateButton (_("Clear Queue"), NULL,
      uimusicqClearQueueProcess, uimusicq);
  g_signal_connect (widget, "clicked",
      G_CALLBACK (uimusicqClearQueueProcess), uimusicq);
  gtk_box_pack_end (GTK_BOX (hbox), widget,
      FALSE, FALSE, 0);

  /* musicq tree view */

  widget = uiutilsCreateScrolledWindow ();
  gtk_box_pack_start (GTK_BOX (uimusicq->ui [ci].box), widget,
      FALSE, FALSE, 0);

  uimusicq->ui [ci].musicqTree = uiutilsCreateTreeView ();
  assert (uimusicq->ui [ci].musicqTree != NULL);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (uimusicq->ui [ci].musicqTree), TRUE);
  gtk_widget_set_halign (uimusicq->ui [ci].musicqTree, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (uimusicq->ui [ci].musicqTree, TRUE);
  gtk_widget_set_vexpand (uimusicq->ui [ci].musicqTree, TRUE);
  gtk_container_add (GTK_CONTAINER (widget), uimusicq->ui [ci].musicqTree);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_renderer_set_alignment (renderer, 1.0, 0.5);
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", MUSICQ_COL_DISP_IDX,
      "font", MUSICQ_COL_FONT,
      NULL);
  gtk_tree_view_column_set_title (column, "");
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_append_column (GTK_TREE_VIEW (uimusicq->ui [ci].musicqTree), column);

  renderer = gtk_cell_renderer_pixbuf_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "pixbuf", MUSICQ_COL_PAUSEIND,
      NULL);
  gtk_tree_view_column_set_title (column, "");
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_append_column (GTK_TREE_VIEW (uimusicq->ui [ci].musicqTree), column);

  sellist = dispselGetList (uimusicq->dispsel, DISP_SEL_MUSICQ);
  uiutilsAddDisplayColumns (uimusicq->ui [ci].musicqTree, sellist, MUSICQ_COL_MAX,
      MUSICQ_COL_FONT, MUSICQ_COL_ELLIPSIZE);

  gtk_tree_view_set_model (GTK_TREE_VIEW (uimusicq->ui [ci].musicqTree), NULL);

  uimusicq->musicqManageIdx = tci;

  logProcEnd (LOG_PROC, "uimusicqActivate", "");
  return uimusicq->ui [ci].box;
}

void
uimusicqMainLoop (uimusicq_t *uimusicq)
{
  int           ci;

  ci = uimusicq->musicqManageIdx;

  /* macos loses the selection after it is moved.  reset it. */
  /* fortunately, it seems to only need to be reset once. */
  if (mstimeCheck (&uimusicq->ui [ci].rowChangeTimer)) {
    if (uimusicq->ui [ci].selPathStr != NULL) {
      uimusicqSetSelection (uimusicq, uimusicq->ui [ci].selPathStr);
    }
    mstimeset (&uimusicq->ui [ci].rowChangeTimer, 3600000);
  }

  return;
}

int
uimusicqProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
    bdjmsgmsg_t msg, char *args, void *udata)
{
  uimusicq_t    *uimusicq = udata;


  switch (route) {
    case ROUTE_NONE:
    case ROUTE_PLAYERUI: {
      switch (msg) {
        case MSG_MUSIC_QUEUE_DATA: {
          uimusicqProcessMusicQueueData (uimusicq, args);
          break;
        }
        case MSG_SONG_SELECT: {
          uimusicqProcessSongSelect (uimusicq, args);
          break;
        }
        default: {
          break;
        }
      }
      break;
    }
    default: {
      break;
    }
  }

  return 0;
}

void
uimusicqSetPlayIdx (uimusicq_t *uimusicq, int playIdx)
{
  uimusicq->musicqPlayIdx = playIdx;
}

void
uimusicqSetManageIdx (uimusicq_t *uimusicq, int manageIdx)
{
  uimusicq->musicqManageIdx = manageIdx;
}

/* internal routines */

static void
uimusicqCreatePlaylistList (uimusicq_t *uimusicq)
{
  int               ci;
  slist_t           *plList;


  logProcBegin (LOG_PROC, "uimusicqCreatePlaylistList");

  ci = uimusicq->musicqManageIdx;

  plList = playlistGetPlaylistList ();
  uiutilsDropDownSetList (&uimusicq->ui [ci].playlistsel, plList, NULL);
  slistFree (plList);
  logProcEnd (LOG_PROC, "uimusicqCreatePlaylistList", "");
}

static void
uimusicqClearQueueProcess (GtkButton *b, gpointer udata)
{
  int           ci;
  int           idx;
  uimusicq_t    *uimusicq = udata;
  char          tbuff [20];


  logProcBegin (LOG_PROC, "uimusicqClearQueueProcess");

  ci = uimusicq->musicqManageIdx;

  idx = uimusicqGetSelection (uimusicq);
  if (idx < 0) {
    /* if nothing is selected, clear everything */
    idx = 0;
    if (uimusicq->musicqManageIdx == uimusicq->musicqPlayIdx) {
      idx = 1;
    }
  }

  snprintf (tbuff, sizeof (tbuff), "%d%c%d", ci, MSG_ARGS_RS, idx);
  connSendMessage (uimusicq->conn, ROUTE_MAIN, MSG_QUEUE_CLEAR, tbuff);
  logProcEnd (LOG_PROC, "uimusicqClearQueueProcess", "");
}

static void
uimusicqQueueDanceProcess (GtkTreeView *tv, GtkTreePath *path,
    GtkTreeViewColumn *column, gpointer udata)
{
  int           ci;
  uimusicq_t    *uimusicq = udata;
  ssize_t       idx;
  char          tbuff [20];


  logProcBegin (LOG_PROC, "uimusicqQueueDanceProcess");

  ci = uimusicq->musicqManageIdx;

  idx = uiutilsDropDownSelectionGet (&uimusicq->ui [ci].dancesel, path);
  if (idx >= 0) {
    snprintf (tbuff, sizeof (tbuff), "%d%c%lu", ci, MSG_ARGS_RS, idx);
    connSendMessage (uimusicq->conn, ROUTE_MAIN, MSG_QUEUE_DANCE, tbuff);
  }
  logProcEnd (LOG_PROC, "uimusicqQueueDanceProcess", "");
}

static void
uimusicqQueuePlaylistProcess (GtkTreeView *tv, GtkTreePath *path,
    GtkTreeViewColumn *column, gpointer udata)
{
  int           ci;
  uimusicq_t    *uimusicq = udata;
  ssize_t       idx;
  char          tbuff [100];


  logProcBegin (LOG_PROC, "uimusicqQueuePlaylistProcess");

  ci = uimusicq->musicqManageIdx;

  idx = uiutilsDropDownSelectionGet (&uimusicq->ui [ci].playlistsel, path);
  if (idx >= 0) {
    snprintf (tbuff, sizeof (tbuff), "%d%c%s", ci, MSG_ARGS_RS,
        uimusicq->ui [ci].playlistsel.strSelection);
    connSendMessage (uimusicq->conn, ROUTE_MAIN, MSG_QUEUE_PLAYLIST, tbuff);
  }
  logProcEnd (LOG_PROC, "uimusicqQueuePlaylistProcess", "");
}

static void
uimusicqProcessSongSelect (uimusicq_t *uimusicq, char * args)
{
  uimusicqSetSelection (uimusicq, args);
}

static void
uimusicqProcessMusicQueueData (uimusicq_t *uimusicq, char * args)
{
  int               ci;
  GtkTreeModel      *model;


  logProcBegin (LOG_PROC, "uimusicqProcessMusicQueueData");

  ci = uimusicq->musicqManageIdx;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (uimusicq->ui [ci].musicqTree));
  if (model == NULL) {
    uimusicqProcessMusicQueueDataNew (uimusicq, args);
  } else {
    uimusicqProcessMusicQueueDataUpdate (uimusicq, args);
  }
  logProcEnd (LOG_PROC, "uimusicqProcessMusicQueueData", "");
}

static void
uimusicqProcessMusicQueueDataNew (uimusicq_t *uimusicq, char * args)
{
  int               ci;
  song_t            *song = NULL;
  GtkTreeIter       iter;
  GtkListStore      *store = NULL;
  GdkPixbuf         *pixbuf = NULL;
  nlistidx_t        iteridx;
  musicqupdate_t    *musicqupdate = NULL;
  char              *listingFont;
  int               musicqcolcount;
  GType             *musicqstoretypes;
  slist_t           *sellist;


  logProcBegin (LOG_PROC, "uimusicqProcessMusicQueueDataNew");
  listingFont = bdjoptGetStr (OPT_MP_LISTING_FONT);


  musicqstoretypes = malloc (sizeof (GType) * MUSICQ_COL_MAX);
  musicqcolcount = 0;
  /* attributes: ellipsize/align/font */
  musicqstoretypes [musicqcolcount++] = G_TYPE_INT;
  musicqstoretypes [musicqcolcount++] = G_TYPE_STRING;
  /* internal idx/unique idx/dbidx*/
  musicqstoretypes [musicqcolcount++] = G_TYPE_ULONG;
  musicqstoretypes [musicqcolcount++] = G_TYPE_ULONG;
  musicqstoretypes [musicqcolcount++] = G_TYPE_ULONG;
  /* display disp idx/pause ind*/
  musicqstoretypes [musicqcolcount++] = G_TYPE_ULONG;
  musicqstoretypes [musicqcolcount++] = GDK_TYPE_PIXBUF;

  sellist = dispselGetList (uimusicq->dispsel, DISP_SEL_MUSICQ);
  musicqstoretypes = uiutilsAddDisplayTypes (musicqstoretypes, sellist, &musicqcolcount);

  store = gtk_list_store_newv (musicqcolcount, musicqstoretypes);
  assert (store != NULL);
  free (musicqstoretypes);

  ci = uimusicqMusicQueueDataParse (uimusicq, args);

  nlistStartIterator (uimusicq->dispList, &iteridx);
  while ((musicqupdate = nlistIterateValueData (uimusicq->dispList, &iteridx)) != NULL) {
    song = dbGetByIdx (musicqupdate->dbidx);

    pixbuf = NULL;
    if (musicqupdate->pflag) {
      pixbuf = uimusicq->pauseImg;
    }

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
        MUSICQ_COL_ELLIPSIZE, PANGO_ELLIPSIZE_END,
        MUSICQ_COL_FONT, listingFont,
        MUSICQ_COL_IDX, musicqupdate->idx,
        MUSICQ_COL_UNIQUE_IDX, musicqupdate->uniqueidx,
        MUSICQ_COL_DBIDX, musicqupdate->dbidx,
        MUSICQ_COL_DISP_IDX, musicqupdate->dispidx,
        MUSICQ_COL_PAUSEIND, pixbuf,
        -1);

    uimusicqSetMusicqDisplay (uimusicq, store, &iter, song);
  }

  gtk_tree_view_set_model (GTK_TREE_VIEW (uimusicq->ui [ci].musicqTree), GTK_TREE_MODEL (store));

  g_object_unref (G_OBJECT (store));
  nlistFree (uimusicq->dispList);
  nlistFree (uimusicq->uniqueList);
  uimusicq->dispList = NULL;
  uimusicq->uniqueList = NULL;
  logProcEnd (LOG_PROC, "uimusicqProcessMusicQueueDataNew", "");
}


static void
uimusicqProcessMusicQueueDataUpdate (uimusicq_t *uimusicq, char * args)
{
  int               ci;
  unsigned long     tlong;
  GtkTreeModel      *model;
  GtkTreeIter       iter;
  GtkTreeRowReference *rowref;
  musicqupdate_t    *musicqupdate;
  nlistidx_t        iteridx;
  gboolean          valid;
  char              *listingFont;


  logProcBegin (LOG_PROC, "uimusicqProcessMusicQueueDataUpdate");
  listingFont = bdjoptGetStr (OPT_MP_LISTING_FONT);

  uimusicq->workList = nlistAlloc ("temp-musicq-work", LIST_UNORDERED, NULL);

  ci = uimusicqMusicQueueDataParse (uimusicq, args);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (uimusicq->ui [ci].musicqTree));

  if (model == NULL) {
    logProcEnd (LOG_PROC, "uimusicqProcessMusicQueueDataUpdate", "null-model");
    return;
  }

  gtk_tree_model_foreach (model,
      uimusicqMusicQueueDataFindRemovals, uimusicq);

  /* process removals */
  nlistStartIterator (uimusicq->workList, &iteridx);
  while ((rowref = nlistIterateValueData (uimusicq->workList, &iteridx)) != NULL) {
    GtkTreeIter iter;
    GtkTreePath *path;

    path = gtk_tree_row_reference_get_path (rowref);
    if (path) {
      if (gtk_tree_model_get_iter (model, &iter, path)) {
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
      }
      gtk_tree_path_free (path);
    }
  }

  nlistFree (uimusicq->workList);
  uimusicq->workList = NULL;

  valid = gtk_tree_model_get_iter_first (model, &iter);

  nlistStartIterator (uimusicq->dispList, &iteridx);
  while ((musicqupdate = nlistIterateValueData (uimusicq->dispList, &iteridx)) != NULL) {
    song_t        *song;
    GdkPixbuf     *pixbuf;

    pixbuf = NULL;
    if (musicqupdate->pflag) {
      pixbuf = uimusicq->pauseImg;
    }

    if (valid) {
      gtk_tree_model_get (model, &iter, MUSICQ_COL_UNIQUE_IDX, &tlong, -1);
      musicqupdate->uniqueidx = tlong;
    }

    song = dbGetByIdx (musicqupdate->dbidx);

    /* there's no need to determine if the entry is new or not    */
    /* simply overwrite everything until the end of the gtk-store */
    /* is reached, then start appending. */
    if (! valid) {
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
          MUSICQ_COL_ELLIPSIZE, 1,
          MUSICQ_COL_FONT, listingFont,
          MUSICQ_COL_IDX, musicqupdate->idx,
          MUSICQ_COL_DISP_IDX, musicqupdate->dispidx,
          MUSICQ_COL_UNIQUE_IDX, musicqupdate->uniqueidx,
          MUSICQ_COL_DBIDX, musicqupdate->dbidx,
          MUSICQ_COL_PAUSEIND, pixbuf,
          -1);
      uimusicqSetMusicqDisplay (uimusicq, GTK_LIST_STORE (model), &iter, song);
    } else {
      /* all data must be updated */
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
          MUSICQ_COL_IDX, musicqupdate->idx,
          MUSICQ_COL_DISP_IDX, musicqupdate->dispidx,
          MUSICQ_COL_UNIQUE_IDX, musicqupdate->uniqueidx,
          MUSICQ_COL_DBIDX, musicqupdate->dbidx,
          MUSICQ_COL_PAUSEIND, pixbuf,
          -1);
      uimusicqSetMusicqDisplay (uimusicq, GTK_LIST_STORE (model), &iter, song);
    }

    if (valid) {
      valid = gtk_tree_model_iter_next (model, &iter);
    }
  }

  nlistFree (uimusicq->dispList);
  nlistFree (uimusicq->uniqueList);
  uimusicq->dispList = NULL;
  uimusicq->uniqueList = NULL;
  logProcEnd (LOG_PROC, "uimusicqProcessMusicQueueDataUpdate", "");
}

static int
uimusicqMusicQueueDataFindRemovals (GtkTreeModel *model,
    GtkTreePath *path, GtkTreeIter *iter, gpointer udata)
{
  uimusicq_t      *uimusicq = udata;
  unsigned long   uniqueidx;


  logProcBegin (LOG_PROC, "uimusicqMusicQueueDataFindRemovals");

  gtk_tree_model_get (model, iter, MUSICQ_COL_UNIQUE_IDX, &uniqueidx, -1);
  if (nlistGetData (uimusicq->uniqueList, uniqueidx) == NULL) {
    GtkTreeRowReference *rowref;

    rowref = gtk_tree_row_reference_new (model, path);
    nlistSetData (uimusicq->workList, uniqueidx, rowref);
  }

  logProcEnd (LOG_PROC, "uimusicqMusicQueueDataFindRemovals", "");
  return FALSE;
}

static int
uimusicqMusicQueueDataParse (uimusicq_t *uimusicq, char *args)
{
  int               ci;
  char              *p;
  char              *tokstr;
  int               idx;
  musicqupdate_t    *musicqupdate;


  logProcBegin (LOG_PROC, "uimusicqMusicQueueDataParse");

  /* first, build ourselves a list to work with */
  uimusicq->uniqueList = nlistAlloc ("temp-musicq-unique", LIST_ORDERED, free);
  uimusicq->dispList = nlistAlloc ("temp-musicq-disp", LIST_ORDERED, NULL);

  p = strtok_r (args, MSG_ARGS_RS_STR, &tokstr);
  ci = atoi (p);

  /* index 0 is the currently playing song */
  idx = 1;
  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  while (p != NULL) {
    musicqupdate = malloc (sizeof (musicqupdate_t));
    assert (musicqupdate != NULL);

    musicqupdate->dispidx = atoi (p);
    p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
    musicqupdate->uniqueidx = atoi (p);
    p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
    musicqupdate->dbidx = atol (p);
    p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
    musicqupdate->pflag = atoi (p);
    musicqupdate->idx = idx;

    nlistSetData (uimusicq->uniqueList, musicqupdate->uniqueidx, musicqupdate);
    nlistSetData (uimusicq->dispList, musicqupdate->idx, musicqupdate);

    p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
    ++idx;
  }

  logProcEnd (LOG_PROC, "uimusicqMusicQueueDataParse", "");
  return ci;
}


static void
uimusicqMoveTopProcess (GtkButton *b, gpointer udata)
{
  int               ci;
  uimusicq_t        *uimusicq = udata;
  ssize_t           idx;
  char              tbuff [20];


  logProcBegin (LOG_PROC, "uimusicqMoveTopProcess");


  ci = uimusicq->musicqManageIdx;

  idx = uimusicqGetSelection (uimusicq);
  if (idx < 0) {
    logProcEnd (LOG_PROC, "uimusicqMoveTopProcess", "bad-idx");
    return;
  }

  uimusicqMusicQueueSetSelected (uimusicq, ci, UIMUSICQ_SEL_TOP);

  snprintf (tbuff, sizeof (tbuff), "%d%c%lu", ci, MSG_ARGS_RS, idx);
  connSendMessage (uimusicq->conn, ROUTE_MAIN,
      MSG_MUSICQ_MOVE_TOP, tbuff);
  logProcEnd (LOG_PROC, "uimusicqMoveTopProcess", "");
}

static void
uimusicqStopRepeat (GtkButton *b, gpointer udata)
{
  int         ci;
  uimusicq_t  *uimusicq = udata;


  logProcBegin (LOG_PROC, "uimusicqStopRepeat");

  ci = uimusicq->musicqManageIdx;
  if (uimusicq->ui [ci].repeatTimer != 0) {
    g_source_remove (uimusicq->ui [ci].repeatTimer);
  }
  uimusicq->ui [ci].repeatTimer = 0;
  logProcEnd (LOG_PROC, "uimusicqStopRepeat", "");
}


static gboolean
uimusicqMoveUpRepeat (gpointer udata)
{
  uimusicq_t        *uimusicq = udata;

  uimusicqMoveUpProcess (NULL, uimusicq);
  return TRUE;
}

static void
uimusicqMoveUpProcess (GtkButton *b, gpointer udata)
{
  int               ci;
  uimusicq_t        *uimusicq = udata;
  ssize_t           idx;
  char              tbuff [20];


  logProcBegin (LOG_PROC, "uimusicqMoveUpProcess");

  ci = uimusicq->musicqManageIdx;

  idx = uimusicqGetSelection (uimusicq);
  if (idx < 0) {
    logProcEnd (LOG_PROC, "uimusicqMoveUpProcess", "bad-idx");
    return;
  }

  uimusicqMusicQueueSetSelected (uimusicq, ci, UIMUSICQ_SEL_PREV);
  snprintf (tbuff, sizeof (tbuff), "%d%c%lu", ci, MSG_ARGS_RS, idx);
  connSendMessage (uimusicq->conn, ROUTE_MAIN,
      MSG_MUSICQ_MOVE_UP, tbuff);
  if (uimusicq->ui [ci].repeatTimer == 0) {
    uimusicq->ui [ci].repeatTimer = g_timeout_add (UIMUSICQ_REPEAT_TIME, uimusicqMoveUpRepeat, uimusicq);
  }
  logProcEnd (LOG_PROC, "uimusicqMoveUpProcess", "");
}

static gboolean
uimusicqMoveDownRepeat (gpointer udata)
{
  uimusicq_t        *uimusicq = udata;

  uimusicqMoveDownProcess (NULL, uimusicq);
  return TRUE;
}

static void
uimusicqMoveDownProcess (GtkButton *b, gpointer udata)
{
  int               ci;
  uimusicq_t        *uimusicq = udata;
  ssize_t           idx;
  char              tbuff [20];


  logProcBegin (LOG_PROC, "uimusicqMoveDownProcess");

  ci = uimusicq->musicqManageIdx;

  idx = uimusicqGetSelection (uimusicq);
  if (idx < 0) {
    logProcEnd (LOG_PROC, "uimusicqMoveDownProcess", "bad-idx");
    return;
  }

  uimusicqMusicQueueSetSelected (uimusicq, ci, UIMUSICQ_SEL_NEXT);
  snprintf (tbuff, sizeof (tbuff), "%d%c%lu", ci, MSG_ARGS_RS, idx);
  connSendMessage (uimusicq->conn, ROUTE_MAIN,
      MSG_MUSICQ_MOVE_DOWN, tbuff);

  if (uimusicq->ui [ci].repeatTimer == 0) {
    uimusicq->ui [ci].repeatTimer = g_timeout_add (UIMUSICQ_REPEAT_TIME, uimusicqMoveDownRepeat, uimusicq);
  }
  logProcEnd (LOG_PROC, "uimusicqMoveDownProcess", "");
}

static void
uimusicqTogglePauseProcess (GtkButton *b, gpointer udata)
{
  int           ci;
  uimusicq_t    *uimusicq = udata;
  ssize_t       idx;
  char          tbuff [20];


  logProcBegin (LOG_PROC, "uimusicqTogglePauseProcess");

  idx = uimusicqGetSelection (uimusicq);
  if (idx < 0) {
    logProcEnd (LOG_PROC, "uimusicqTogglePauseProcess", "bad-idx");
    return;
  }

  ci = uimusicq->musicqManageIdx;

  snprintf (tbuff, sizeof (tbuff), "%d%c%lu", ci, MSG_ARGS_RS, idx);
  connSendMessage (uimusicq->conn, ROUTE_MAIN,
      MSG_MUSICQ_TOGGLE_PAUSE, tbuff);
  logProcEnd (LOG_PROC, "uimusicqTogglePauseProcess", "");
}

static void
uimusicqRemoveProcess (GtkButton *b, gpointer udata)
{
  int           ci;
  uimusicq_t    *uimusicq = udata;
  ssize_t       idx;
  char          tbuff [20];


  logProcBegin (LOG_PROC, "uimusicqRemoveProcess");

  idx = uimusicqGetSelection (uimusicq);
  if (idx < 0) {
    logProcEnd (LOG_PROC, "uimusicqRemoveProcess", "bad-idx");
    return;
  }

  ci = uimusicq->musicqManageIdx;

  snprintf (tbuff, sizeof (tbuff), "%d%c%lu", ci, MSG_ARGS_RS, idx);
  connSendMessage (uimusicq->conn, ROUTE_MAIN,
      MSG_MUSICQ_REMOVE, tbuff);
  logProcEnd (LOG_PROC, "uimusicqRemoveProcess", "");
}

static void
uimusicqSetSelection (uimusicq_t *uimusicq, char *pathstr)
{
  int               ci;
  GtkTreePath       *path;


  logProcBegin (LOG_PROC, "uimusicqSetSelection");

  ci = uimusicq->musicqManageIdx;

  path = gtk_tree_path_new_from_string (pathstr);
  if (path != NULL) {
    gtk_tree_view_set_cursor (GTK_TREE_VIEW (uimusicq->ui [ci].musicqTree),
        path, NULL, FALSE);
    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (uimusicq->ui [ci].musicqTree),
        path, NULL, FALSE, 0.0, 0.0);
    gtk_tree_path_free (path);
  }
  logProcEnd (LOG_PROC, "uimusicqSetSelection", "");
}

static ssize_t
uimusicqGetSelection (uimusicq_t *uimusicq)
{
  int               ci;
  GtkTreeSelection  *sel;
  GtkTreeIter       iter;
  GtkTreeModel      *model;
  ssize_t           idx;
  unsigned long     tidx;
  gint              count;


  logProcBegin (LOG_PROC, "uimusicqGetSelection");

  ci = uimusicq->musicqManageIdx;

  sel = gtk_tree_view_get_selection (
      GTK_TREE_VIEW (uimusicq->ui [ci].musicqTree));
  count = gtk_tree_selection_count_selected_rows (sel);
  if (count != 1) {
    logProcEnd (LOG_PROC, "uimusicqGetSelection", "count != 1");
    return -1;
  }
  gtk_tree_selection_get_selected (sel, &model, &iter);
  gtk_tree_model_get (model, &iter, MUSICQ_COL_IDX, &tidx, -1);
  idx = tidx;

  logProcEnd (LOG_PROC, "uimusicqGetSelection", "");
  return idx;
}

static void
uimusicqMusicQueueSetSelected (uimusicq_t *uimusicq, int ci, int which)
{
  GtkTreeModel      *model;
  gboolean          valid;
  GtkTreeSelection  *sel;
  GtkTreeIter       iter;
  gint              count;


  logProcBegin (LOG_PROC, "uimusicqMusicQueueSetSelected");

  sel = gtk_tree_view_get_selection (
      GTK_TREE_VIEW (uimusicq->ui [ci].musicqTree));
  count = gtk_tree_selection_count_selected_rows (sel);
  if (count != 1) {
    logProcEnd (LOG_PROC, "uimusicqMusicQueueSetSelected", "count != 1");
    return;
  }
  gtk_tree_selection_get_selected (sel, &model, &iter);

  switch (which) {
    case UIMUSICQ_SEL_PREV: {
      valid = gtk_tree_model_iter_previous (model, &iter);
      break;
    }
    case UIMUSICQ_SEL_NEXT: {
      valid = gtk_tree_model_iter_next (model, &iter);
      break;
    }
    case UIMUSICQ_SEL_TOP: {
      valid = gtk_tree_model_get_iter_first (model, &iter);
      break;
    }
  }

  if (valid) {
    GtkTreePath *path;

    gtk_tree_selection_select_iter (sel, &iter);
    path = gtk_tree_model_get_path (model, &iter);
    uimusicq->ui [ci].selPathStr = gtk_tree_path_to_string (path);
    /* macos loses the selection */
    /* set a timer to re-select it */
    mstimeset (&uimusicq->ui [ci].rowChangeTimer, 50);
    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (uimusicq->ui [ci].musicqTree),
        path, NULL, FALSE, 0.0, 0.0);
    gtk_tree_path_free (path);
  }
  logProcEnd (LOG_PROC, "uimusicqMusicQueueSetSelected", "");
}


static void
uimusicqSetMusicqDisplay (uimusicq_t *uimusicq, GtkListStore *store,
    GtkTreeIter *iter, song_t *song)
{
  slist_t     *sellist;

  sellist = dispselGetList (uimusicq->dispsel, DISP_SEL_MUSICQ);
  uiutilsSetDisplayColumns (store, iter, sellist, song, MUSICQ_COL_MAX);
}