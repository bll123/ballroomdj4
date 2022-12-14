#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>
#include <sys/time.h> // for mongoose
#include <dirent.h> // for mongoose

/* winsock2.h should come before windows.h */
#if _hdr_winsock2
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wmissing-declarations"
# include <winsock2.h>
# pragma clang diagnostic pop
#endif
#if _hdr_windows
# include <windows.h>       // for mongoose
#endif

#include "mongoose.h"

#include "bdj4.h"
#include "bdj4init.h"
#include "bdjmsg.h"
#include "bdjopt.h"
#include "bdjvars.h"
#include "conn.h"
#include "fileop.h"
#include "lock.h"
#include "log.h"
#include "ossignal.h"
#include "pathbld.h"
#include "progstate.h"
#include "sockh.h"
#include "sysvars.h"
#include "tagdef.h"
#include "tmutil.h"
#include "websrv.h"

typedef struct {
  conn_t          *conn;
  progstate_t     *progstate;
  int             stopwaitcount;
  char            *locknm;
  uint16_t        port;
  bdjmobilemq_t   type;
  char            *name;
  char            *title;
  websrv_t        *websrv;
  char            *marqueeData;
} mobmqdata_t;

static bool     mobmqConnectingCallback (void *tmmdata, programstate_t programState);
static bool     mobmqHandshakeCallback (void *tmmdata, programstate_t programState);
static bool     mobmqStoppingCallback (void *tmmdata, programstate_t programState);
static bool     mobmqStopWaitCallback (void *tmmdata, programstate_t programState);
static bool     mobmqClosingCallback (void *tmmdata, programstate_t programState);
static void     mobmqEventHandler (struct mg_connection *c, int ev,
                    void *ev_data, void *userdata);
static int      mobmqProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
                    bdjmsgmsg_t msg, char *args, void *udata);
static int      mobmqProcessing (void *udata);
static void     mobmqSigHandler (int sig);

static int  gKillReceived = 0;

int
main (int argc, char *argv[])
{
  mobmqdata_t     mobmqData;
  uint16_t        listenPort;
  char            *tval;
  int             flags;

  osSetStandardSignals (mobmqSigHandler);

  flags = BDJ4_INIT_NO_DB_LOAD | BDJ4_INIT_NO_DATAFILE_LOAD;
  bdj4startup (argc, argv, NULL, "mm", ROUTE_MOBILEMQ, flags);
  mobmqData.conn = connInit (ROUTE_MARQUEE);

  mobmqData.type = (bdjmobilemq_t) bdjoptGetNum (OPT_P_MOBILEMARQUEE);
  if (mobmqData.type == MOBILEMQ_OFF) {
    lockRelease (mobmqData.locknm, PATHBLD_MP_USEIDX);
    exit (0);
  }

  mobmqData.progstate = progstateInit ("mobilemq");
  progstateSetCallback (mobmqData.progstate, STATE_CONNECTING,
      mobmqConnectingCallback, &mobmqData);
  progstateSetCallback (mobmqData.progstate, STATE_WAIT_HANDSHAKE,
      mobmqHandshakeCallback, &mobmqData);
  progstateSetCallback (mobmqData.progstate, STATE_STOPPING,
      mobmqStoppingCallback, &mobmqData);
  progstateSetCallback (mobmqData.progstate, STATE_STOP_WAIT,
      mobmqStopWaitCallback, &mobmqData);
  progstateSetCallback (mobmqData.progstate, STATE_CLOSING,
      mobmqClosingCallback, &mobmqData);
  mobmqData.port = bdjoptGetNum (OPT_P_MOBILEMQPORT);
  mobmqData.name = NULL;
  tval = bdjoptGetStr (OPT_P_MOBILEMQTAG);
  if (tval != NULL) {
    mobmqData.name = strdup (tval);
  }
  tval = bdjoptGetStr (OPT_P_MOBILEMQTITLE);
  mobmqData.title = NULL;
  if (tval != NULL) {
    mobmqData.title = strdup (tval);
  }
  mobmqData.websrv = NULL;
  mobmqData.marqueeData = NULL;
  mobmqData.stopwaitcount = 0;

  mobmqData.websrv = websrvInit (mobmqData.port, mobmqEventHandler, &mobmqData);

  listenPort = bdjvarsGetNum (BDJVL_MOBILEMQ_PORT);
  sockhMainLoop (listenPort, mobmqProcessMsg, mobmqProcessing, &mobmqData);
  connFree (mobmqData.conn);
  progstateFree (mobmqData.progstate);
  logEnd ();
  return 0;
}

/* internal routines */

static bool
mobmqStoppingCallback (void *tmmdata, programstate_t programState)
{
  mobmqdata_t   *mobmqData = tmmdata;

  connDisconnect (mobmqData->conn, ROUTE_MAIN);
  return STATE_FINISHED;
}

static bool
mobmqStopWaitCallback (void *tmobmq, programstate_t programState)
{
  mobmqdata_t *mobmq = tmobmq;
  bool        rc;

  rc = connWaitClosed (mobmq->conn, &mobmq->stopwaitcount);
  return rc;
}

static bool
mobmqClosingCallback (void *tmmdata, programstate_t programState)
{
  mobmqdata_t   *mobmqData = tmmdata;

  bdj4shutdown (ROUTE_MOBILEMQ, NULL);

  websrvFree (mobmqData->websrv);
  if (mobmqData->name != NULL) {
    free (mobmqData->name);
  }
  if (mobmqData->title != NULL) {
    free (mobmqData->title);
  }
  if (mobmqData->marqueeData != NULL) {
    free (mobmqData->marqueeData);
  }

  return STATE_FINISHED;
}

static void
mobmqEventHandler (struct mg_connection *c, int ev, void *ev_data, void *userdata)
{
  mobmqdata_t   *mobmqData = userdata;
  char          *data = NULL;
  char          *title = NULL;
  char          tbuff [400];

  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;

    if (mg_http_match_uri (hm, "/mmupdate")) {
      data = mobmqData->marqueeData;
      title = mobmqData->title;
      if (data == NULL) {
        if (title == NULL) {
          title = "";
        }
        snprintf (tbuff, sizeof (tbuff),
            "{ "
            "\"title\" : \"%s\","
            "\"current\" : \"Not Playing\","
            "\"skip\" : \"true\""
            "}",
            title);
        data = tbuff;
      }
      mg_http_reply (c, 200, "Content-Type: application/json\r\n",
          "%s\r\n", data);
    } else if (mg_http_match_uri (hm, "#.key") ||
        mg_http_match_uri (hm, "#.crt") ||
        mg_http_match_uri (hm, "#.pem") ||
        mg_http_match_uri (hm, "#.csr") ||
        mg_http_match_uri (hm, "../")) {
      mg_http_reply (c, 403, NULL, "%s", "Forbidden");
    } else {
      char          tbuff [200];
      char          tbuffb [MAXPATHLEN];
      struct mg_str turi;
      struct mg_str tmpuri;
      bool          reloc = false;

      struct mg_http_serve_opts opts = { .root_dir = sysvarsGetStr (SV_BDJ4HTTPDIR) };
      snprintf (tbuff, sizeof (tbuff), "%.*s", (int) hm->uri.len, hm->uri.ptr);
      pathbldMakePath (tbuffb, sizeof (tbuffb),
          tbuff, "", PATHBLD_MP_HTTPDIR);
      if (! fileopFileExists (tbuffb)) {
        turi = mg_str ("/mobilemq.html");
        reloc = true;
      }

      if (reloc) {
        tmpuri = hm->uri;
        hm->uri = turi;
      }
      logMsg (LOG_DBG, LOG_IMPORTANT, "serve: %.*s", (int) hm->uri.len, hm->uri.ptr);
      mg_http_serve_dir (c, hm, &opts);
      if (reloc) {
        hm->uri = tmpuri;
      }
    }
  }
}

static int
mobmqProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
    bdjmsgmsg_t msg, char *args, void *udata)
{
  mobmqdata_t     *mobmqData = udata;

  logMsg (LOG_DBG, LOG_MSGS, "got: from:%d/%s route:%d/%s msg:%d/%s args:%s",
      routefrom, msgRouteDebugText (routefrom),
      route, msgRouteDebugText (route), msg, msgDebugText (msg), args);

  switch (route) {
    case ROUTE_NONE:
    case ROUTE_MOBILEMQ: {
      switch (msg) {
        case MSG_HANDSHAKE: {
          connProcessHandshake (mobmqData->conn, routefrom);
          break;
        }
        case MSG_EXIT_REQUEST: {
          logMsg (LOG_SESS, LOG_IMPORTANT, "got exit request");
          progstateShutdownProcess (mobmqData->progstate);
          break;
        }
        case MSG_MARQUEE_DATA: {
          if (mobmqData->marqueeData != NULL) {
            free (mobmqData->marqueeData);
          }
          mobmqData->marqueeData = strdup (args);
          assert (mobmqData->marqueeData != NULL);
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
mobmqProcessing (void *udata)
{
  mobmqdata_t     *mobmqData = udata;
  websrv_t        *websrv = mobmqData->websrv;
  int           stop = false;


  if (! progstateIsRunning (mobmqData->progstate)) {
    progstateProcess (mobmqData->progstate);
    if (progstateCurrState (mobmqData->progstate) == STATE_CLOSED) {
      stop = true;
    }
    if (gKillReceived) {
      progstateShutdownProcess (mobmqData->progstate);
      logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
    }
    return stop;
  }

  connProcessUnconnected (mobmqData->conn);

  websrvProcess (websrv);

  if (gKillReceived) {
    progstateShutdownProcess (mobmqData->progstate);
    logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
  }
  return stop;
}

static bool
mobmqConnectingCallback (void *tmmdata, programstate_t programState)
{
  mobmqdata_t   *mobmqData = tmmdata;
  bool          rc = STATE_NOT_FINISH;

  connProcessUnconnected (mobmqData->conn);

  if (! connIsConnected (mobmqData->conn, ROUTE_MAIN)) {
    connConnect (mobmqData->conn, ROUTE_MAIN);
  }

  if (connIsConnected (mobmqData->conn, ROUTE_MAIN)) {
    rc = STATE_FINISHED;
  }

  return rc;
}

static bool
mobmqHandshakeCallback (void *tmmdata, programstate_t programState)
{
  mobmqdata_t   *mobmqData = tmmdata;
  bool          rc = STATE_NOT_FINISH;

  connProcessUnconnected (mobmqData->conn);

  if (connHaveHandshake (mobmqData->conn, ROUTE_MAIN)) {
    rc = STATE_FINISHED;
  }

  return rc;
}

static void
mobmqSigHandler (int sig)
{
  gKillReceived = 1;
}
