/*
 *  bdj4dbtag
 *  reads the tags from an audio file.
 *  uses pthreads.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <pthread.h>

#include "bdj4.h"
#include "bdjmsg.h"
#include "bdjopt.h"
#include "bdjstring.h"
#include "bdjvarsdf.h"
#include "bdjvars.h"
#include "conn.h"
#include "filedata.h"
#include "lock.h"
#include "log.h"
#include "pathbld.h"
#include "progstate.h"
#include "procutil.h"
#include "queue.h"
#include "sockh.h"
#include "sysvars.h"

#define DBTAG_MAX_THREADS   6
#define DBTAG_TMP_FN        "dbtag"

enum {
  DBTAG_T_STATE_INIT,
  DBTAG_T_STATE_ACTIVE,
  DBTAG_T_STATE_HAVE_DATA,
};

typedef struct {
} dbtagdata_t;

typedef struct {
  pthread_t   thread;
  int         state;
  int         idx;
  char        *fn;
  char        tmpfn [40];
  char        *tagdata;
} dbthread_t;

typedef struct {
  progstate_t       *progstate;
  procutil_t        *processes [ROUTE_MAX];
  conn_t            *conn;
  char              *locknm;
  int               numActiveThreads;
  queue_t           *fileQueue;
  dbthread_t     threads [DBTAG_MAX_THREADS];
} dbtag_t;

static int      dbtagProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
                    bdjmsgmsg_t msg, char *args, void *udata);
static int      dbtagProcessing (void *udata);
static bool     dbtagConnectingCallback (void *tdbtag, programstate_t programState);
static bool     dbtagHandshakeCallback (void *tdbtag, programstate_t programState);
static bool     dbtagStoppingCallback (void *tdbtag, programstate_t programState);
static bool     dbtagClosingCallback (void *tdbtag, programstate_t programState);
static void     dbtagProcessFileMsg (dbtag_t *dbtag, char *args);
static void     * dbtagProcessFile (void *tdbthread);
static void     dbtagSigHandler (int sig);

static int  gKillReceived = 0;

int
main (int argc, char *argv[])
{
  dbtag_t     dbtag;
  uint16_t    listenPort;
  loglevel_t  loglevel = LOG_IMPORTANT | LOG_MAIN;
  int         rc;
  int         c;
  int         option_index;

  static struct option bdj_options [] = {
    { "debug",      required_argument,  NULL,   'd' },
    { "profile",    required_argument,  NULL,   'p' },
    { NULL,         0,                  NULL,   0 }
  };

#if _define_SIGHUP
  procutilCatchSignal (dbtagSigHandler, SIGHUP);
#endif
  procutilCatchSignal (dbtagSigHandler, SIGINT);
  procutilCatchSignal (dbtagSigHandler, SIGTERM);
#if _define_SIGCHLD
  procutilIgnoreSignal (SIGCHLD);
#endif

  sysvarsInit (argv[0]);

  while ((c = getopt_long_only (argc, argv, "p:d:", bdj_options, &option_index)) != -1) {
    switch (c) {
      case 'd': {
        if (optarg) {
          loglevel = (loglevel_t) atoi (optarg);
        }
        break;
      }
      case 'p': {
        if (optarg) {
          sysvarsSetNum (SVL_BDJIDX, atol (optarg));
        }
        break;
      }
      default: {
        break;
      }
    }
  }

  logStartAppend ("dbtag", "dt", loglevel);
  logProcBegin (LOG_PROC, "dbtag");

  dbtag.locknm = lockName (ROUTE_DBTAG);
  rc = lockAcquire (dbtag.locknm, PATHBLD_MP_USEIDX);
  if (rc < 0) {
    logMsg (LOG_DBG, LOG_IMPORTANT, "ERR: dbtag: unable to acquire lock: profile: %zd", sysvarsGetNum (SVL_BDJIDX));
    logMsg (LOG_SESS, LOG_IMPORTANT, "ERR: dbtag: unable to acquire lock: profile: %zd", sysvarsGetNum (SVL_BDJIDX));
    logEnd ();
    exit (0);
  }

  dbtag.numActiveThreads = 0;
  dbtag.progstate = progstateInit ("dbtag");
  for (int i = 0; i < DBTAG_MAX_THREADS; ++i) {
    char tbuff [40];
    char buff [10];

    dbtag.threads [i].state = DBTAG_T_STATE_INIT;
    dbtag.threads [i].idx = i;
    dbtag.threads [i].thread = -1;
    dbtag.threads [i].fn = NULL;
    snprintf (buff, sizeof (buff), "%s-%d", DBTAG_TMP_FN, i);
    pathbldMakePath (tbuff, sizeof (tbuff), "",
        buff, ".txt", PATHBLD_MP_TMPDIR);
    strlcpy (dbtag.threads [i].tmpfn, tbuff, sizeof (dbtag.threads [i].tmpfn));
    dbtag.threads [i].tagdata = NULL;
  }
  dbtag.fileQueue = queueAlloc (free);

  progstateSetCallback (dbtag.progstate, STATE_CONNECTING,
      dbtagConnectingCallback, &dbtag);
  progstateSetCallback (dbtag.progstate, STATE_WAIT_HANDSHAKE,
      dbtagHandshakeCallback, &dbtag);
  progstateSetCallback (dbtag.progstate, STATE_STOPPING,
      dbtagStoppingCallback, &dbtag);
  progstateSetCallback (dbtag.progstate, STATE_CLOSING,
      dbtagClosingCallback, &dbtag);

  for (bdjmsgroute_t i = ROUTE_NONE; i < ROUTE_MAX; ++i) {
    dbtag.processes [i] = NULL;
  }

  bdjvarsInit ();
  bdjoptInit ();

  dbtag.conn = connInit (ROUTE_DBTAG);

  listenPort = bdjvarsGetNum (BDJVL_DBTAG_PORT);
  sockhMainLoop (listenPort, dbtagProcessMsg, dbtagProcessing, &dbtag);

  while (progstateShutdownProcess (dbtag.progstate) != STATE_CLOSED) {
    ;
  }
  progstateFree (dbtag.progstate);

  logProcEnd (LOG_PROC, "dbtag", "");
  logEnd ();
  return 0;
}

/* internal routines */

static int
dbtagProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
    bdjmsgmsg_t msg, char *args, void *udata)
{
  dbtag_t      *dbtag;

  dbtag = (dbtag_t *) udata;

  switch (route) {
    case ROUTE_NONE:
    case ROUTE_DBTAG: {
      switch (msg) {
        case MSG_HANDSHAKE: {
          connProcessHandshake (dbtag->conn, routefrom);
          connConnectResponse (dbtag->conn, routefrom);
          break;
        }
        case MSG_SOCKET_CLOSE: {
          connDisconnect (dbtag->conn, routefrom);
          break;
        }
        case MSG_EXIT_REQUEST: {
          logMsg (LOG_SESS, LOG_IMPORTANT, "got exit request");
          gKillReceived = 0;
          progstateShutdownProcess (dbtag->progstate);
          logProcEnd (LOG_PROC, "dbtagProcessMsg", "req-exit");
          return 1;
        }
        case MSG_DB_FILE_CHK: {
          dbtagProcessFileMsg (dbtag, args);
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

static int
dbtagProcessing (void *udata)
{
  dbtag_t      *dbtag = NULL;

  dbtag = (dbtag_t *) udata;

  if (! progstateIsRunning (dbtag->progstate)) {
    progstateProcess (dbtag->progstate);
    if (gKillReceived) {
      logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
    }
    return gKillReceived;
  }

  for (int i = 0; i < DBTAG_MAX_THREADS; ++i) {
    if (dbtag->threads [i].state == DBTAG_T_STATE_HAVE_DATA) {
fprintf (stderr, "got data: %s\n", dbtag->threads [i].tagdata);
      // ### process the returned data, and send it back to
      // the dbupdate process.
      connSendMessage (dbtag->conn, ROUTE_DBUPDATE, MSG_DB_FILE_TAGS,
          dbtag->threads [i].fn);
      free (dbtag->threads [i].fn);
      dbtag->threads [i].fn = NULL;
      free (dbtag->threads [i].tagdata);
      dbtag->threads [i].tagdata = NULL;
      dbtag->threads [i].state = DBTAG_T_STATE_INIT;
      --dbtag->numActiveThreads;
    }
  }

  while (queueGetCount (dbtag->fileQueue) > 0 &&
      dbtag->numActiveThreads < DBTAG_MAX_THREADS) {
    for (int i = 0; i < DBTAG_MAX_THREADS; ++i) {
      if (dbtag->threads [i].state == DBTAG_T_STATE_INIT) {
        char    *fn;

        fn = queuePop (dbtag->fileQueue);
        if (fn == NULL) {
          break;
        }

        dbtag->threads [i].state = DBTAG_T_STATE_ACTIVE;
        ++dbtag->numActiveThreads;
        if (dbtag->threads [i].fn != NULL) {
          free (dbtag->threads [i].fn);
          dbtag->threads [i].fn = NULL;
        }
        dbtag->threads [i].fn = strdup (fn);
        pthread_create (&dbtag->threads [i].thread, NULL, dbtagProcessFile, &dbtag->threads [i]);
      }
    }
  }

  if (gKillReceived) {
    logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
  }
  return gKillReceived;
}

static bool
dbtagConnectingCallback (void *tdbtag, programstate_t programState)
{
  dbtag_t *dbtag = tdbtag;
  bool    rc = false;

  logProcBegin (LOG_PROC, "dbtagConnectingCallback");

  if (! connIsConnected (dbtag->conn, ROUTE_DBUPDATE)) {
    connConnect (dbtag->conn, ROUTE_DBUPDATE);
  }

  if (connIsConnected (dbtag->conn, ROUTE_DBUPDATE)) {
    rc = true;
  }

  logProcEnd (LOG_PROC, "dbtagConnectingCallback", "");
  return rc;
}

static bool
dbtagHandshakeCallback (void *tdbtag, programstate_t programState)
{
  dbtag_t    *dbtag = tdbtag;
  bool          rc = false;

  logProcBegin (LOG_PROC, "dbtagHandshakeCallback");

  if (connHaveHandshake (dbtag->conn, ROUTE_DBUPDATE)) {
    rc = true;
  }
  logProcEnd (LOG_PROC, "dbtagHandshakeCallback", "");
  return rc;
}

static bool
dbtagStoppingCallback (void *tdbtag, programstate_t programState)
{
  dbtag_t    *dbtag = tdbtag;

  logProcBegin (LOG_PROC, "dbtagStoppingCallback");
  connDisconnectAll (dbtag->conn);
  logProcEnd (LOG_PROC, "dbtagStoppingCallback", "");
  return true;
}

static bool
dbtagClosingCallback (void *tdbtag, programstate_t programState)
{
  dbtag_t    *dbtag = tdbtag;

  logProcBegin (LOG_PROC, "dbtagClosingCallback");

  connFree (dbtag->conn);
  lockRelease (dbtag->locknm, PATHBLD_MP_USEIDX);

  logProcEnd (LOG_PROC, "dbtagClosingCallback", "");
  return true;
}

static void
dbtagProcessFileMsg (dbtag_t *dbtag, char *args)
{
  queuePush (dbtag->fileQueue, args);
fprintf (stderr, "push: %s\n", args);
}

static void *
dbtagProcessFile (void *tdbthread)
{
  dbthread_t    * dbthread = tdbthread;
  char          cmd [MAXPATHLEN];
  char          *data;

  snprintf (cmd, sizeof (cmd), "%s %s \"%s\" > %s",
      sysvarsGetStr (SV_PYTHON_PATH), sysvarsGetStr (SV_PYTHON_MUTAGEN),
      dbthread->fn, dbthread->tmpfn);
fprintf (stderr, "cmd: %s\n", cmd);
  system (cmd);
  data = filedataReadAll (dbthread->tmpfn);
fprintf (stderr, "data: %s\n", cmd);
  dbthread->tagdata = data;
  dbthread->state = DBTAG_T_STATE_HAVE_DATA;
  return NULL;
}

static void
dbtagSigHandler (int sig)
{
  gKillReceived = 1;
}
