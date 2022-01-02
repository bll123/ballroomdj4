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
#include "bdjstring.h"
#include "sysvars.h"
#include "portability.h"

typedef struct {
  programstate_t   programState;
  int              playerStarted;
  Sock_t           playerSocket;
} maindata_t;

static void     startPlayer (maindata_t *mainData);
static void     stopPlayer (maindata_t *mainData);
static int      processMsg (long route, long msg, char *args, void *udata);
static void     mainProcessing (void *udata);

int
main (int argc, char *argv[])
{
  maindata_t    mainData;

  mainData.programState = STATE_INITIALIZING;
  mainData.playerStarted = 0;
  mainData.playerSocket = INVALID_SOCKET;

  bdj4startup (argc, argv);

  mainData.programState = STATE_RUNNING;
  uint16_t listenPort = bdjvarsl [BDJVL_MAIN_PORT];
  sockhMainLoop (listenPort, processMsg, mainProcessing, &mainData);
  bdj4shutdown ();
  mainData.programState = STATE_NOT_RUNNING;
  return 0;
}

/* internal routines */

static void
startPlayer (maindata_t *mainData)
{
  char      tbuff [MAXPATHLEN];
  pid_t     pid;

  if (mainData->playerStarted) {
    return;
  }

  logProcBegin (LOG_LVL_4, "startPlayer");
  snprintf (tbuff, MAXPATHLEN, "%s/bdj4player", sysvars [SV_BDJ4EXECDIR]);
  if (isWindows()) {
    strlcat (tbuff, ".exe", MAXPATHLEN);
  }
  int rc = processStart (tbuff, &pid, lsysvars [SVL_BDJIDX]);
  if (rc < 0) {
    logMsg (LOG_DBG, LOG_LVL_4, "player %s failed to start", tbuff);
  } else {
    logMsg (LOG_DBG, LOG_LVL_4, "player started pid: %zd", (ssize_t) pid);
    mainData->playerStarted = 1;
  }
  logProcEnd (LOG_LVL_4, "startPlayer", "");
}

static void
stopPlayer (maindata_t *mainData)
{
  if (! mainData->playerStarted) {
    return;
  }
  sockhSendMessage (mainData->playerSocket, ROUTE_PLAYER, MSG_REQUEST_EXIT, NULL);
  sockhSendMessage (mainData->playerSocket, ROUTE_PLAYER, MSG_CLOSE_SOCKET, NULL);
  sockClose (mainData->playerSocket);
  mainData->playerSocket = INVALID_SOCKET;
}

static int
processMsg (long route, long msg, char *args, void *udata)
{
  maindata_t      *mainData;

  logProcBegin (LOG_LVL_4, "processMsg");
  mainData = (maindata_t *) udata;

  logMsg (LOG_DBG, LOG_LVL_4, "got: route: %ld msg:%ld args:%s", route, msg, args);
  switch (route) {
    case ROUTE_NONE:
    case ROUTE_MAIN: {
      switch (msg) {
        case MSG_PLAYER_START: {
          logMsg (LOG_DBG, LOG_LVL_4, "got: start-player");
          startPlayer (mainData);
          break;
        }
        case MSG_REQUEST_EXIT: {
          mainData->programState = STATE_CLOSING;
          stopPlayer (mainData);
          logProcEnd (LOG_LVL_4, "processMsg", "req-exit");
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

  logProcEnd (LOG_LVL_4, "processMsg", "");
  return 0;
}

static void
mainProcessing (void *udata)
{
  maindata_t      *mainData;

  mainData = (maindata_t *) udata;

  if (mainData->programState == STATE_RUNNING &&
      mainData->playerStarted &&
      mainData->playerSocket == INVALID_SOCKET) {
    int       err;

    uint16_t playerPort = bdjvarsl [BDJVL_PLAYER_PORT];
    mainData->playerSocket = sockConnect (playerPort, &err, 1000);
    logMsg (LOG_DBG, LOG_LVL_4, "player-socket: %zd",
        (size_t) mainData->playerSocket);
  }

  return;
}
