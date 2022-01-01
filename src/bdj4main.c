#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "bdj4init.h"
#include "sockh.h"
#include "sock.h"
#include "bdjvars.h"
#include "bdjmsg.h"
#include "log.h"
#include "process.h"
#include "portability.h"
#include "bdjstring.h"
#include "sysvars.h"

static void     startPlayer (void);
static void     stopPlayer (void);
static int      processMsg (long route, long msg, char *args);
static void     mainProcessing (void);

static programstate_t   programState = STATE_NOT_RUNNING;
static int              playerStarted = 0;
static Sock_t           playerSocket = INVALID_SOCKET;

int
main (int argc, char *argv[])
{
  programState = STATE_INITIALIZING;
  bdj4startup (argc, argv);

  programState = STATE_RUNNING;
  uint16_t listenPort = lbdjvars [BDJVL_MAIN_PORT];
  sockhMainLoop (listenPort, processMsg, mainProcessing);
  bdj4shutdown ();
  programState = STATE_NOT_RUNNING;
  return 0;
}

/* internal routines */

static void
startPlayer (void)
{
  char      tbuff [MAXPATHLEN];
  pid_t     pid;

  if (playerStarted) {
    return;
  }

  logProcBegin (LOG_LVL_4, "startPlayer");
  snprintf (tbuff, MAXPATHLEN, "%s/bdj4player", sysvars [SV_BDJ4EXECDIR]);
  if (isWindows()) {
    strlcat (tbuff, ".exe", MAXPATHLEN);
  }
  int rc = processStart (tbuff, &pid);
  if (rc < 0) {
    logMsg (LOG_DBG, LOG_LVL_4, "player %s failed to start", tbuff);
  } else {
    logMsg (LOG_DBG, LOG_LVL_4, "player started pid: %zd", (ssize_t) pid);
    playerStarted = 1;
  }
  logProcEnd (LOG_LVL_4, "startPlayer", "");
}

static void
stopPlayer (void)
{
  if (! playerStarted) {
    return;
  }
  sockhSendMessage (playerSocket, ROUTE_PLAYER, MSG_REQUEST_EXIT, NULL);
  sockhSendMessage (playerSocket, ROUTE_PLAYER, MSG_CLOSE_SOCKET, NULL);
  sockClose (playerSocket);
  playerSocket = INVALID_SOCKET;
}

static int
processMsg (long route, long msg, char *args)
{
  logProcBegin (LOG_LVL_4, "processMsg");
  logMsg (LOG_DBG, LOG_LVL_4, "got: route: %ld msg:%ld", route, msg);
  switch (route) {
    case ROUTE_NONE: {
      break;
    }
    case ROUTE_PLAYER: {
      break;
    }
    case ROUTE_MAIN: {
      logMsg (LOG_DBG, LOG_LVL_4, "got: route-main");
      switch (msg) {
        case MSG_PLAYER_START: {
          logMsg (LOG_DBG, LOG_LVL_4, "got: start-player");
          startPlayer ();
          break;
        }
        case MSG_REQUEST_EXIT: {
          programState = STATE_CLOSING;
          stopPlayer ();
          logProcEnd (LOG_LVL_4, "processMsg", "req-exit");
          return 1;
        }
        default: {
          break;
        }
      }
      break;
    }
    case ROUTE_GUI: {
      break;
    }
    default: {
      break;
    }
  }

  logProcEnd (LOG_LVL_4, "processMsg", "");
  return 0;
}

static void
mainProcessing (void)
{
  if (programState == STATE_RUNNING &&
      playerStarted &&
      playerSocket == INVALID_SOCKET) {
    int       err;

    uint16_t playerPort = lbdjvars [BDJVL_PLAYER_PORT];
    playerSocket = sockConnect (playerPort, &err, 1000);
    logMsg (LOG_DBG, LOG_LVL_4, "player-socket: %zd", (size_t) playerSocket);
  }

  return;
}
