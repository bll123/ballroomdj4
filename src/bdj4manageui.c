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

#include <gtk/gtk.h>

#include "bdj4.h"
#include "bdj4init.h"
#include "bdj4intl.h"
#include "bdj4playerui.h"
#include "bdjmsg.h"
#include "bdjopt.h"
#include "bdjstring.h"
#include "bdjvars.h"
#include "bdjvarsdfload.h"
#include "conn.h"
#include "datafile.h"
#include "dispsel.h"
#include "localeutil.h"
#include "lock.h"
#include "log.h"
#include "musicq.h"
#include "osuiutils.h"
#include "pathbld.h"
#include "procutil.h"
#include "progstate.h"
#include "slist.h"
#include "sock.h"
#include "sockh.h"
#include "sysvars.h"
#include "tmutil.h"
#include "uimusicq.h"
#include "uiplayer.h"
#include "uisongsel.h"
#include "uiutils.h"

typedef struct {
  progstate_t     *progstate;
  char            *locknm;
  procutil_t      *processes [ROUTE_MAX];
  conn_t          *conn;
  sockserver_t    *sockserver;
  musicqidx_t     musicqPlayIdx;
  musicqidx_t     musicqManageIdx;
  dispsel_t       *dispsel;
  int             dbgflags;
  /* gtk stuff */
  GtkApplication  *app;
  GtkWidget       *window;
  GtkWidget       *mainnotebook;
  GtkWidget       *vbox;
  GtkWidget       *notebook;
  /* ui major elements */
  uiplayer_t      *uiplayer;
  uimusicq_t      *uimusicq;
  uisongsel_t     *uisongsel;
  /* options */
  datafile_t      *optiondf;
  nlist_t         *options;
} manageui_t;

/* re-use the plui enums so that the songsel filter enums can also be used */
static datafilekey_t manageuidfkeys [] = {
  { "FILTER_POS_X",     SONGSEL_FILTER_POSITION_X,  VALUE_NUM, NULL, -1 },
  { "FILTER_POS_Y",     SONGSEL_FILTER_POSITION_Y,  VALUE_NUM, NULL, -1 },
  { "PLUI_POS_X",       PLUI_POSITION_X,            VALUE_NUM, NULL, -1 },
  { "PLUI_POS_Y",       PLUI_POSITION_Y,            VALUE_NUM, NULL, -1 },
  { "PLUI_SIZE_X",      PLUI_SIZE_X,                VALUE_NUM, NULL, -1 },
  { "PLUI_SIZE_Y",      PLUI_SIZE_Y,                VALUE_NUM, NULL, -1 },
  { "SORT_BY",          SONGSEL_SORT_BY,            VALUE_STR, NULL, -1 },
};
#define MANAGEUI_DFKEY_COUNT (sizeof (manageuidfkeys) / sizeof (datafilekey_t))

#define MANAGE_EXIT_WAIT_COUNT      20

static bool     manageListeningCallback (void *udata, programstate_t programState);
static bool     manageConnectingCallback (void *udata, programstate_t programState);
static bool     manageHandshakeCallback (void *udata, programstate_t programState);
static bool     manageStoppingCallback (void *udata, programstate_t programState);
static bool     manageClosingCallback (void *udata, programstate_t programState);
static void     manageActivate (GApplication *app, gpointer userdata);
gboolean        manageMainLoop  (void *tmanage);
static int      manageProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
                    bdjmsgmsg_t msg, char *args, void *udata);
static gboolean manageCloseWin (GtkWidget *window, GdkEvent *event, gpointer userdata);
static void     manageSigHandler (int sig);


static int gKillReceived = 0;
static int gdone = 0;

int
main (int argc, char *argv[])
{
  int             status = 0;
  uint16_t        listenPort;
  manageui_t      manage;
  char            *uifont;
  char            tbuff [MAXPATHLEN];


  manage.mainnotebook = NULL;
  manage.notebook = NULL;
  manage.progstate = progstateInit ("manageui");
  progstateSetCallback (manage.progstate, STATE_LISTENING,
      manageListeningCallback, &manage);
  progstateSetCallback (manage.progstate, STATE_CONNECTING,
      manageConnectingCallback, &manage);
  progstateSetCallback (manage.progstate, STATE_WAIT_HANDSHAKE,
      manageHandshakeCallback, &manage);
  manage.sockserver = NULL;
  manage.window = NULL;
  manage.uiplayer = NULL;
  manage.uimusicq = NULL;
  manage.uisongsel = NULL;
  manage.musicqPlayIdx = MUSICQ_B;
  manage.musicqManageIdx = MUSICQ_A;

  for (bdjmsgroute_t i = ROUTE_NONE; i < ROUTE_MAX; ++i) {
    manage.processes [i] = NULL;
  }

#if _define_SIGHUP
  procutilCatchSignal (manageSigHandler, SIGHUP);
#endif
  procutilCatchSignal (manageSigHandler, SIGINT);
  procutilDefaultSignal (SIGTERM);
#if _define_SIGCHLD
  procutilDefaultSignal (SIGCHLD);
#endif

  manage.dbgflags = bdj4startup (argc, argv, "mu", ROUTE_MANAGEUI, BDJ4_INIT_NONE);
  logProcBegin (LOG_PROC, "manageui");

  manage.dispsel = dispselAlloc ();

  listenPort = bdjvarsGetNum (BDJVL_MANAGEUI_PORT);
  manage.conn = connInit (ROUTE_MANAGEUI);

  pathbldMakePath (tbuff, sizeof (tbuff),
      "manageui", ".txt", PATHBLD_MP_USEIDX);
  manage.optiondf = datafileAllocParse ("manageui-opt", DFTYPE_KEY_VAL, tbuff,
      manageuidfkeys, MANAGEUI_DFKEY_COUNT, DATAFILE_NO_LOOKUP);
  manage.options = datafileGetList (manage.optiondf);
  if (manage.options == NULL) {
    manage.options = nlistAlloc ("manageui-opt", LIST_ORDERED, free);

    nlistSetNum (manage.options, SONGSEL_FILTER_POSITION_X, -1);
    nlistSetNum (manage.options, SONGSEL_FILTER_POSITION_Y, -1);
    nlistSetNum (manage.options, PLUI_POSITION_X, -1);
    nlistSetNum (manage.options, PLUI_POSITION_Y, -1);
    nlistSetNum (manage.options, PLUI_SIZE_X, 1000);
    nlistSetNum (manage.options, PLUI_SIZE_Y, 600);
    nlistSetStr (manage.options, SONGSEL_SORT_BY, "TITLE");
  }

  manage.uiplayer = uiplayerInit (manage.progstate, manage.conn);
  manage.uimusicq = uimusicqInit (manage.progstate, manage.conn, manage.dispsel,
      UIMUSICQ_FLAGS_NO_QUEUE | UIMUSICQ_FLAGS_NO_TOGGLE_PAUSE,
      DISP_SEL_SONGLIST);
  manage.uisongsel = uisongselInit (manage.progstate, manage.conn, manage.dispsel,
      manage.options, SONG_FILTER_FOR_SELECTION, UISONGSEL_FLAGS_NO_Q_BUTTON,
      DISP_SEL_SONGSEL);

  /* register these after calling the sub-window initialization */
  /* then these will be run last, after the other closing callbacks */
  progstateSetCallback (manage.progstate, STATE_STOPPING,
      manageStoppingCallback, &manage);
  progstateSetCallback (manage.progstate, STATE_CLOSING,
      manageClosingCallback, &manage);

  manage.sockserver = sockhStartServer (listenPort);

  uiutilsInitUILog ();
  gtk_init (&argc, NULL);
  uifont = bdjoptGetStr (OPT_MP_UIFONT);
  uiutilsSetUIFont (uifont);

  g_timeout_add (UI_MAIN_LOOP_TIMER, manageMainLoop, &manage);

  status = uiutilsCreateApplication (0, NULL, "manageui",
      &manage.app, manageActivate, &manage);

  while (progstateShutdownProcess (manage.progstate) != STATE_CLOSED) {
    ;
  }

  progstateFree (manage.progstate);
  logProcEnd (LOG_PROC, "manageui", "");
  logEnd ();
  return status;
}

/* internal routines */

static bool
manageStoppingCallback (void *udata, programstate_t programState)
{
  manageui_t    * manage = udata;
  gint          x, y;

  logProcBegin (LOG_PROC, "manageStoppingCallback");
  gtk_window_get_size (GTK_WINDOW (manage->window), &x, &y);
  nlistSetNum (manage->options, PLUI_SIZE_X, x);
  nlistSetNum (manage->options, PLUI_SIZE_Y, y);
  gtk_window_get_position (GTK_WINDOW (manage->window), &x, &y);
  nlistSetNum (manage->options, PLUI_POSITION_X, x);
  nlistSetNum (manage->options, PLUI_POSITION_Y, y);

  for (bdjmsgroute_t i = ROUTE_NONE; i < ROUTE_MAX; ++i) {
    if (manage->processes [i] != NULL) {
      procutilStopProcess (manage->processes [i], manage->conn, i, false);
    }
  }

  gdone = 1;
  connDisconnectAll (manage->conn);
  logProcEnd (LOG_PROC, "manageStoppingCallback", "");
  return true;
}

static bool
manageClosingCallback (void *udata, programstate_t programState)
{
  manageui_t    *manage = udata;
  char          fn [MAXPATHLEN];

  logProcBegin (LOG_PROC, "manageClosingCallback");

  if (GTK_IS_WIDGET (manage->window)) {
    gtk_widget_destroy (manage->window);
  }

  pathbldMakePath (fn, sizeof (fn),
      "manageui", ".txt", PATHBLD_MP_USEIDX);
  datafileSaveKeyVal ("manageui", fn, manageuidfkeys, MANAGEUI_DFKEY_COUNT, manage->options);

  sockhCloseServer (manage->sockserver);

  bdj4shutdown (ROUTE_MANAGEUI);
  dispselFree (manage->dispsel);

  if (manage->options != datafileGetList (manage->optiondf)) {
    nlistFree (manage->options);
  }
  datafileFree (manage->optiondf);

  /* give the other processes some time to shut down */
  mssleep (200);
  for (bdjmsgroute_t i = ROUTE_NONE; i < ROUTE_MAX; ++i) {
    if (manage->processes [i] != NULL) {
      procutilStopProcess (manage->processes [i], manage->conn, i, true);
      procutilFree (manage->processes [i]);
    }
  }

  connFree (manage->conn);

  uiplayerFree (manage->uiplayer);
  uimusicqFree (manage->uimusicq);
  uisongselFree (manage->uisongsel);
  uiutilsCleanup ();

  logProcEnd (LOG_PROC, "manageClosingCallback", "");
  return true;
}

static void
manageActivate (GApplication *app, gpointer userdata)
{
  manageui_t          *manage = userdata;
  GError              *gerr = NULL;
  GtkWidget           *tabLabel;
  GtkWidget           *widget;
  GtkWidget           *hbox;
  GtkWidget           *vbox;
  char                imgbuff [MAXPATHLEN];
  char                tbuff [MAXPATHLEN];
  gint                x, y;

  logProcBegin (LOG_PROC, "manageActivate");

  manage->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  assert (manage->window != NULL);
  gtk_window_set_application (GTK_WINDOW (manage->window), GTK_APPLICATION (app));
  gtk_window_set_application (GTK_WINDOW (manage->window), manage->app);
  gtk_window_set_default_icon_from_file (imgbuff, &gerr);
  g_signal_connect (manage->window, "delete-event", G_CALLBACK (manageCloseWin), manage);
  /* CONTEXT: management ui window title */
  snprintf (tbuff, sizeof (tbuff), _("%s Management"), BDJ4_NAME);
  gtk_window_set_title (GTK_WINDOW (manage->window), tbuff);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (manage->window), vbox);
  gtk_widget_set_margin_top (vbox, 4);
  gtk_widget_set_margin_bottom (vbox, 4);
  gtk_widget_set_margin_start (vbox, 4);
  gtk_widget_set_margin_end (vbox, 4);

  manage->mainnotebook = uiutilsCreateNotebook ();
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (manage->mainnotebook), GTK_POS_LEFT);
  gtk_box_pack_start (GTK_BOX (vbox), manage->mainnotebook,
      FALSE, FALSE, 0);

  manage->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_margin_top (manage->vbox, 4);
  gtk_widget_set_margin_bottom (manage->vbox, 4);
  gtk_widget_set_margin_start (manage->vbox, 4);
  gtk_widget_set_margin_end (manage->vbox, 4);

  tabLabel = uiutilsCreateLabel (_("Song List Editor"));
  uiutilsNotebookAppendPage (manage->mainnotebook, manage->vbox, tabLabel);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  tabLabel = uiutilsCreateLabel (_("Music Manager"));
  uiutilsNotebookAppendPage (manage->mainnotebook, vbox, tabLabel);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  tabLabel = uiutilsCreateLabel (_("Database Update"));
  uiutilsNotebookAppendPage (manage->mainnotebook, vbox, tabLabel);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  tabLabel = uiutilsCreateLabel (_("Playlist Management"));
  uiutilsNotebookAppendPage (manage->mainnotebook, vbox, tabLabel);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  tabLabel = uiutilsCreateLabel (_("Sequences"));
  uiutilsNotebookAppendPage (manage->mainnotebook, vbox, tabLabel);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  tabLabel = uiutilsCreateLabel (_("File Manager"));
  uiutilsNotebookAppendPage (manage->mainnotebook, vbox, tabLabel);

  /* player */
  widget = uiplayerActivate (manage->uiplayer);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_pack_start (GTK_BOX (manage->vbox), widget,
      FALSE, FALSE, 0);

  /* there doesn't seem to be any other good method to identify which */
  /* notebook page is which.  The code is dependent on musicq_a being */
  /* in tab 0, */
  manage->notebook = uiutilsCreateNotebook ();
  gtk_box_pack_start (GTK_BOX (manage->vbox), manage->notebook, TRUE, TRUE, 0);

  /* music queue tab */
  widget = uimusicqActivate (manage->uimusicq, manage->window, 0);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  /* CONTEXT: name of song list editor tab */
  tabLabel = uiutilsCreateLabel (_("Song List"));
  uiutilsNotebookAppendPage (manage->notebook, widget, tabLabel);
  gtk_widget_show_all (hbox);

  /* queue B tab */
  widget = uimusicqActivate (manage->uimusicq, manage->window, 1);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  /* this tab is never shown */
  tabLabel = uiutilsCreateLabel ("Queue B");
  uiutilsNotebookAppendPage (manage->notebook, widget, tabLabel);
  gtk_widget_show_all (hbox);

  /* song selection tab */
  widget = uisongselActivate (manage->uisongsel, manage->window);
  /* CONTEXT: name of song selection tab */
  tabLabel = uiutilsCreateLabel (_("Song Selection"));
  uiutilsNotebookAppendPage (manage->notebook, widget, tabLabel);

  x = nlistGetNum (manage->options, PLUI_SIZE_X);
  y = nlistGetNum (manage->options, PLUI_SIZE_Y);
  gtk_window_set_default_size (GTK_WINDOW (manage->window), x, y);

  gtk_widget_show_all (manage->window);

  x = nlistGetNum (manage->options, PLUI_POSITION_X);
  y = nlistGetNum (manage->options, PLUI_POSITION_Y);
  if (x != -1 && y != -1) {
    gtk_window_move (GTK_WINDOW (manage->window), x, y);
  }

  pathbldMakePath (imgbuff, sizeof (imgbuff),
      "bdj4_icon", ".png", PATHBLD_MP_IMGDIR);
  osuiSetIcon (imgbuff);

  logProcEnd (LOG_PROC, "manageActivate", "");
}

gboolean
manageMainLoop (void *tmanage)
{
  manageui_t   *manage = tmanage;
  int         tdone = 0;
  gboolean    cont = TRUE;

  tdone = sockhProcessMain (manage->sockserver, manageProcessMsg, manage);
  if (tdone || gdone) {
    ++gdone;
  }
  if (gdone > MANAGE_EXIT_WAIT_COUNT) {
    g_application_quit (G_APPLICATION (manage->app));
    cont = FALSE;
  }

  if (! progstateIsRunning (manage->progstate)) {
    progstateProcess (manage->progstate);
    if (gKillReceived) {
      logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
      progstateShutdownProcess (manage->progstate);
    }
    return cont;
  }

  uiplayerMainLoop (manage->uiplayer);
  uimusicqMainLoop (manage->uimusicq);
  uisongselMainLoop (manage->uisongsel);

  if (gKillReceived) {
    logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
    progstateShutdownProcess (manage->progstate);
  }
  return cont;
}

static bool
manageListeningCallback (void *udata, programstate_t programState)
{
  manageui_t    *manage = udata;
  int           flags;

  logProcBegin (LOG_PROC, "manageListeningCallback");
  flags = PROCUTIL_DETACH;
  if ((manage->dbgflags & BDJ4_INIT_NO_DETACH) == BDJ4_INIT_NO_DETACH) {
    flags = PROCUTIL_NO_DETACH;
  }
  manage->processes [ROUTE_MAIN] = procutilStartProcess (
      ROUTE_MAIN, "bdj4main", flags);
  logProcEnd (LOG_PROC, "manageListeningCallback", "");
  return true;
}

static bool
manageConnectingCallback (void *udata, programstate_t programState)
{
  manageui_t   *manage = udata;
  bool        rc = false;

  logProcBegin (LOG_PROC, "manageConnectingCallback");

  if (! connIsConnected (manage->conn, ROUTE_MAIN)) {
    connConnect (manage->conn, ROUTE_MAIN);
  }
  if (! connIsConnected (manage->conn, ROUTE_PLAYER)) {
    connConnect (manage->conn, ROUTE_PLAYER);
  }

  if (connIsConnected (manage->conn, ROUTE_MAIN) &&
      connIsConnected (manage->conn, ROUTE_PLAYER)) {
    rc = true;
  }

  logProcEnd (LOG_PROC, "manageConnectingCallback", "");
  return rc;
}

static bool
manageHandshakeCallback (void *udata, programstate_t programState)
{
  manageui_t   *manage = udata;
  bool          rc = false;

  logProcBegin (LOG_PROC, "manageHandshakeCallback");

  if (connHaveHandshake (manage->conn, ROUTE_MAIN) &&
      connHaveHandshake (manage->conn, ROUTE_PLAYER)) {
    progstateLogTime (manage->progstate, "time-to-start-gui");
    rc = true;
  }

  logProcEnd (LOG_PROC, "manageHandshakeCallback", "");
  return rc;
}

static int
manageProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
    bdjmsgmsg_t msg, char *args, void *udata)
{
  manageui_t       *manage = udata;

  logProcBegin (LOG_PROC, "manageProcessMsg");

  logMsg (LOG_DBG, LOG_MSGS, "got: from:%ld/%s route:%ld/%s msg:%ld/%s args:%s",
      routefrom, msgRouteDebugText (routefrom),
      route, msgRouteDebugText (route), msg, msgDebugText (msg), args);

  uiplayerProcessMsg (routefrom, route, msg, args, manage->uiplayer);
  uimusicqProcessMsg (routefrom, route, msg, args, manage->uimusicq);

  switch (route) {
    case ROUTE_NONE:
    case ROUTE_MANAGEUI: {
      switch (msg) {
        case MSG_HANDSHAKE: {
          connProcessHandshake (manage->conn, routefrom);
          connConnectResponse (manage->conn, routefrom);
          break;
        }
        case MSG_EXIT_REQUEST: {
          logMsg (LOG_SESS, LOG_IMPORTANT, "got exit request");
          gKillReceived = 0;
          logMsg (LOG_DBG, LOG_MSGS, "got: req-exit");
          progstateShutdownProcess (manage->progstate);
          logProcEnd (LOG_PROC, "manageProcessMsg", "req-exit");
          return 1;
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

  if (gKillReceived) {
    logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
  }
  logProcEnd (LOG_PROC, "manageProcessMsg", "");
  return gKillReceived;
}


static gboolean
manageCloseWin (GtkWidget *window, GdkEvent *event, gpointer userdata)
{
  manageui_t   *manage = userdata;

  logProcBegin (LOG_PROC, "manageCloseWin");
  if (! gdone) {
    progstateShutdownProcess (manage->progstate);
    logMsg (LOG_DBG, LOG_MSGS, "got: close win request");
    logProcEnd (LOG_PROC, "manageCloseWin", "not-done");
    return TRUE;
  }

  logProcEnd (LOG_PROC, "manageCloseWin", "");
  return FALSE;
}

static void
manageSigHandler (int sig)
{
  gKillReceived = 1;
}
