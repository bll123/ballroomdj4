/*
 * bdj4main
 *  Main entry point for the player process.
 *  Handles startup of the player, mobile marquee and remote control.
 *  Handles playlists and the music queue.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "bdj4.h"
#include "bdj4init.h"
#include "bdj4intl.h"
#include "bdjmsg.h"
#include "bdjopt.h"
#include "bdjstring.h"
#include "bdjvarsdf.h"
#include "bdjvars.h"
#include "conn.h"
#include "dance.h"
#include "dancesel.h"
#include "filedata.h"
#include "fileop.h"
#include "lock.h"
#include "log.h"
#include "musicdb.h"
#include "musicq.h"
#include "osprocess.h"
#include "ossignal.h"
#include "osutils.h"
#include "pathbld.h"
#include "player.h"
#include "playlist.h"
#include "procutil.h"
#include "progstate.h"
#include "queue.h"
#include "slist.h"
#include "sock.h"
#include "sockh.h"
#include "songsel.h"
#include "sysvars.h"
#include "tagdef.h"
#include "tmutil.h"
#include "webclient.h"

typedef enum {
  MOVE_UP = -1,
  MOVE_DOWN = 1,
} mainmove_t;

enum {
  MAIN_PB_TYPE_PL,
  MAIN_PB_TYPE_SONG,
  MAIN_CHG_CLEAR,
  MAIN_CHG_START,
  MAIN_CHG_FINAL,
  MAIN_PREP_SIZE = 5,
  MAIN_NOT_SET = -1,
};

typedef struct {
  playlist_t        *playlist;
  int               playlistIdx;
} playlistitem_t;

typedef struct {
  progstate_t       *progstate;
  procutil_t        *processes [ROUTE_MAX];
  conn_t            *conn;
  int               startflags;
  musicdb_t         *musicdb;
  /* playlistCache contains all of the playlists that are used */
  /* so that playlist lookups can be processed */
  nlist_t           *playlistCache;
  queue_t           *playlistQueue [MUSICQ_MAX];
  musicq_t          *musicQueue;
  nlist_t           *danceCounts;
  musicqidx_t       musicqPlayIdx;
  musicqidx_t       musicqManageIdx;
  int               musicqDeferredPlayIdx;
  slist_t           *announceList;
  playerstate_t     playerState;
  ssize_t           gap;
  webclient_t       *webclient;
  char              *mobmqUserkey;
  int               stopwaitcount;
  char              *pbfinishArgs;
  int               pbfinishType;
  bdjmsgroute_t     pbfinishRoute;
  int               pbfinishrcv;
  long              ploverridestoptime;
  int               songplaysentcount;        // for testsuite
  int               musicqChanged [MUSICQ_MAX];
  bool              marqueeChanged [MUSICQ_MAX];
  bool              changeSuspend [MUSICQ_MAX];
  bool              playWhenQueued : 1;
  bool              switchQueueWhenEmpty : 1;
  bool              finished : 1;
  bool              marqueestarted : 1;
  bool              waitforpbfinish : 1;
} maindata_t;

static int  mainProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route, bdjmsgmsg_t msg, char *args, void *udata);
static int  mainProcessing (void *udata);
static bool mainListeningCallback (void *tmaindata, programstate_t programState);
static bool mainConnectingCallback (void *tmaindata, programstate_t programState);
static bool mainHandshakeCallback (void *tmaindata, programstate_t programState);
static void mainStartMarquee (maindata_t *mainData);
static bool mainStoppingCallback (void *tmaindata, programstate_t programState);
static bool mainStopWaitCallback (void *tmaindata, programstate_t programState);
static bool mainClosingCallback (void *tmaindata, programstate_t programState);
static void mainSendMusicQueueData (maindata_t *mainData, int musicqidx);
static void mainSendMarqueeData (maindata_t *mainData);
static char * mainSongGetDanceDisplay (maindata_t *mainData, ssize_t idx);
static void mainSendMobileMarqueeData (maindata_t *mainData);
static void mainMobilePostCallback (void *userdata, char *resp, size_t len);
static void mainQueueClear (maindata_t *mainData, char *args);
static void mainQueueDance (maindata_t *mainData, char *args, ssize_t count);
static void mainQueuePlaylist (maindata_t *mainData, char *plname);
static void mainSigHandler (int sig);
static void mainMusicQueueFill (maindata_t *mainData);
static void mainMusicQueuePrep (maindata_t *mainData);
static char *mainPrepSong (maindata_t *maindata, song_t *song, char *sfname, int playlistIdx, bdjmsgprep_t flag);
static void mainTogglePause (maindata_t *mainData, char *args);
static void mainMusicqMove (maindata_t *mainData, char *args, mainmove_t direction);
static void mainMusicqMoveTop (maindata_t *mainData, char *args);
static void mainMusicqClear (maindata_t *mainData, char *args);
static void mainMusicqRemove (maindata_t *mainData, char *args);
static void mainNextSong (maindata_t *mainData);
static void mainMusicqInsert (maindata_t *mainData, bdjmsgroute_t route, char *args);
static void mainMusicqSetManage (maindata_t *mainData, char *args);
static void mainMusicqSetPlayback (maindata_t *mainData, char *args);
static void mainMusicqSwitch (maindata_t *mainData, musicqidx_t newidx);
static void mainPlaybackBegin (maindata_t *mainData);
static void mainMusicQueuePlay (maindata_t *mainData);
static void mainMusicQueueFinish (maindata_t *mainData, const char *args);
static void mainMusicQueueNext (maindata_t *mainData, const char *args);
static ilistidx_t mainMusicQueueHistory (void *mainData, ilistidx_t idx);
static void mainSendDanceList (maindata_t *mainData, bdjmsgroute_t route);
static void mainSendPlaylistList (maindata_t *mainData, bdjmsgroute_t route);
static void mainSendPlayerStatus (maindata_t *mainData, char *playerResp);
static void mainSendMusicqStatus (maindata_t *mainData);
static void mainDanceCountsInit (maindata_t *mainData);
static void mainParseIntNum (char *args, int *a, ilistidx_t *b);
static void mainParseIntStr (char *args, int *a, char **b);
static void mainSendFinished (maindata_t *mainData);
static long mainCalculateSongDuration (maindata_t *mainData, song_t *song, int playlistIdx);
static playlistitem_t * mainPlaylistItemCache (maindata_t *mainData, playlist_t *pl, int playlistIdx);
static void mainPlaylistItemFree (void *tplitem);
static void mainMusicqSetSuspend (maindata_t *mainData, char *args, bool value);
static void mainMusicQueueMix (maindata_t *mainData, char *args);
static void mainPlaybackFinishProcess (maindata_t *mainData, const char *args);
static void mainPlaybackSendSongFinish (maindata_t *mainData, const char *args);
static void mainStatusRequest (maindata_t *mainData, bdjmsgroute_t routefrom);
static void mainChkMusicq (maindata_t *mainData, bdjmsgroute_t routefrom);

static long globalCounter = 0;
static int  gKillReceived = 0;

int
main (int argc, char *argv[])
{
  maindata_t    mainData;
  uint16_t      listenPort;


  mainData.progstate = progstateInit ("main");
  progstateSetCallback (mainData.progstate, STATE_LISTENING,
      mainListeningCallback, &mainData);
  progstateSetCallback (mainData.progstate, STATE_CONNECTING,
      mainConnectingCallback, &mainData);
  progstateSetCallback (mainData.progstate, STATE_WAIT_HANDSHAKE,
      mainHandshakeCallback, &mainData);
  progstateSetCallback (mainData.progstate, STATE_STOPPING,
      mainStoppingCallback, &mainData);
  progstateSetCallback (mainData.progstate, STATE_STOP_WAIT,
      mainStopWaitCallback, &mainData);
  progstateSetCallback (mainData.progstate, STATE_CLOSING,
      mainClosingCallback, &mainData);

  mainData.playlistCache = NULL;
  mainData.musicQueue = NULL;
  mainData.danceCounts = NULL;
  mainData.musicqPlayIdx = MUSICQ_PB_A;
  mainData.musicqManageIdx = MUSICQ_PB_A;
  mainData.musicqDeferredPlayIdx = MAIN_NOT_SET;
  mainData.playerState = PL_STATE_STOPPED;
  mainData.webclient = NULL;
  mainData.mobmqUserkey = NULL;
  mainData.pbfinishArgs = NULL;
  mainData.playWhenQueued = true;
  mainData.switchQueueWhenEmpty = false;
  mainData.finished = false;
  mainData.marqueestarted = false;
  mainData.waitforpbfinish = false;
  mainData.pbfinishrcv = 0;
  mainData.stopwaitcount = 0;
  mainData.ploverridestoptime = 0;
  mainData.songplaysentcount = 0;
  for (musicqidx_t i = 0; i < MUSICQ_MAX; ++i) {
    mainData.playlistQueue [i] = NULL;
    mainData.musicqChanged [i] = MAIN_CHG_CLEAR;
    mainData.marqueeChanged [i] = false;
    mainData.changeSuspend [i] = false;
  }
  procutilInitProcesses (mainData.processes);

  osSetStandardSignals (mainSigHandler);

  mainData.startflags = bdj4startup (argc, argv, &mainData.musicdb,
      "m", ROUTE_MAIN, BDJ4_INIT_NONE);
  logProcBegin (LOG_PROC, "main");

  if (bdjoptGetNum (OPT_P_MOBILEMARQUEE) == MOBILEMQ_INTERNET) {
    mainData.webclient = webclientAlloc (&mainData, mainMobilePostCallback);
  }

  mainData.conn = connInit (ROUTE_MAIN);
  mainData.gap = bdjoptGetNum (OPT_P_GAP);
  mainData.playlistCache = nlistAlloc ("playlist-list", LIST_ORDERED,
      playlistFree);
  for (musicqidx_t i = 0; i < MUSICQ_MAX; ++i) {
    mainData.playlistQueue [i] = queueAlloc (mainPlaylistItemFree);
  }
  mainData.musicQueue = musicqAlloc (mainData.musicdb);
  mainDanceCountsInit (&mainData);
  mainData.announceList = slistAlloc ("announcements", LIST_ORDERED, NULL);

  listenPort = bdjvarsGetNum (BDJVL_MAIN_PORT);
  sockhMainLoop (listenPort, mainProcessMsg, mainProcessing, &mainData);
  connFree (mainData.conn);
  progstateFree (mainData.progstate);
  logProcEnd (LOG_PROC, "main", "");
  logEnd ();
  return 0;
}

/* internal routines */

static bool
mainStoppingCallback (void *tmaindata, programstate_t programState)
{
  maindata_t    *mainData = tmaindata;

  logProcBegin (LOG_PROC, "mainStoppingCallback");

  procutilStopAllProcess (mainData->processes, mainData->conn, false);
  connDisconnect (mainData->conn, ROUTE_STARTERUI);
  logProcEnd (LOG_PROC, "mainStoppingCallback", "");
  return STATE_FINISHED;
}

static bool
mainStopWaitCallback (void *tmaindata, programstate_t programState)
{
  maindata_t  *mainData = tmaindata;
  bool        rc;

  rc = connWaitClosed (mainData->conn, &mainData->stopwaitcount);
  return rc;
}

static bool
mainClosingCallback (void *tmaindata, programstate_t programState)
{
  maindata_t    *mainData = tmaindata;
  char          *script;

  logProcBegin (LOG_PROC, "mainClosingCallback");

  if (mainData->playlistCache != NULL) {
    /* the playlists get freed here */
    nlistFree (mainData->playlistCache);
  }
  for (musicqidx_t i = 0; i < MUSICQ_MAX; ++i) {
    if (mainData->playlistQueue [i] != NULL) {
      queueFree (mainData->playlistQueue [i]);
    }
  }
  if (mainData->musicQueue != NULL) {
    musicqFree (mainData->musicQueue);
  }
  if (mainData->announceList != NULL) {
    slistFree (mainData->announceList);
  }
  if (mainData->webclient != NULL) {
    webclientClose (mainData->webclient);
  }
  if (mainData->mobmqUserkey != NULL) {
    free (mainData->mobmqUserkey);
  }
  if (mainData->danceCounts != NULL) {
    nlistFree (mainData->danceCounts);
  }
  if (mainData->pbfinishArgs != NULL) {
    free (mainData->pbfinishArgs);
  }

  procutilStopAllProcess (mainData->processes, mainData->conn, true);
  procutilFreeAll (mainData->processes);

  script = bdjoptGetStr (OPT_M_SHUTDOWNSCRIPT);
  if (script != NULL &&
      *script &&
      fileopFileExists (script)) {
    const char  *targv [2];

    targv [0] = script;
    targv [1] = NULL;
    osProcessStart (targv, OS_PROC_DETACH, NULL, NULL);
  }

  bdj4shutdown (ROUTE_MAIN, mainData->musicdb);

  logProcEnd (LOG_PROC, "mainClosingCallback", "");
  return STATE_FINISHED;
}

static int
mainProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
    bdjmsgmsg_t msg, char *args, void *udata)
{
  maindata_t  *mainData;
  bool        dbgdisp = false;
  char        *targs = NULL;

  mainData = (maindata_t *) udata;

  if (args != NULL) {
    targs = strdup (args);
  }

  switch (route) {
    case ROUTE_NONE:
    case ROUTE_MAIN: {
      switch (msg) {
        case MSG_HANDSHAKE: {
          connProcessHandshake (mainData->conn, routefrom);
          dbgdisp = true;
          break;
        }
        case MSG_SOCKET_CLOSE: {
          procutilCloseProcess (mainData->processes [routefrom],
              mainData->conn, routefrom);
          procutilFreeRoute (mainData->processes, routefrom);
          connDisconnect (mainData->conn, routefrom);
          dbgdisp = true;
          break;
        }
        case MSG_EXIT_REQUEST: {
          logMsg (LOG_SESS, LOG_IMPORTANT, "got exit request");
          gKillReceived = 0;
          progstateShutdownProcess (mainData->progstate);
          dbgdisp = true;
          break;
        }
        case MSG_PLAYLIST_CLEARPLAY: {
          char  *ttargs;

          ttargs = strdup (targs);
          mainQueueClear (mainData, ttargs);
          free (ttargs);
          mainNextSong (mainData);
          if (mainData->waitforpbfinish) {
            mainData->pbfinishArgs = strdup (targs);
            mainData->pbfinishType = MAIN_PB_TYPE_PL;
          } else {
            mainQueuePlaylist (mainData, targs);
          }
          dbgdisp = true;
          break;
        }
        case MSG_QUEUE_CLEAR: {
          /* clears both the playlist queue and the music queue */
          logMsg (LOG_DBG, LOG_MSGS, "got: queue-clear");
          mainQueueClear (mainData, targs);
          dbgdisp = true;
          break;
        }
        case MSG_QUEUE_CLEAR_PLAY: {
          char  *ttargs;

          /* clears both the playlist queue and the music queue */
          /* does a next song and starts playing */
          logMsg (LOG_DBG, LOG_MSGS, "got: queue-clear-play");
          ttargs = strdup (targs);
          mainQueueClear (mainData, ttargs);
          free (ttargs);
          mainNextSong (mainData);
          if (mainData->waitforpbfinish) {
            mainData->pbfinishArgs = strdup (targs);
            mainData->pbfinishType = MAIN_PB_TYPE_SONG;
            mainData->pbfinishRoute = routefrom;
          } else {
            mainMusicqInsert (mainData, routefrom, targs);
          }
          dbgdisp = true;
          break;
        }
        case MSG_QUEUE_PLAYLIST: {
          logMsg (LOG_DBG, LOG_MSGS, "got: playlist-queue %s", targs);
          mainQueuePlaylist (mainData, targs);
          dbgdisp = true;
          break;
        }
        case MSG_PL_OVERRIDE_STOP_TIME: {
          /* this message overrides the stop time for the */
          /* next queue-playlist message */
          mainData->ploverridestoptime = atol (targs);
          dbgdisp = true;
          break;
        }
        case MSG_QUEUE_DANCE: {
          mainQueueDance (mainData, targs, 1);
          dbgdisp = true;
          break;
        }
        case MSG_QUEUE_DANCE_5: {
          mainQueueDance (mainData, targs, 5);
          dbgdisp = true;
          break;
        }
        case MSG_QUEUE_PLAY_ON_ADD: {
          mainData->playWhenQueued = atoi (targs);
          dbgdisp = true;
          break;
        }
        case MSG_QUEUE_SWITCH_EMPTY: {
          mainData->switchQueueWhenEmpty = atoi (targs);
          dbgdisp = true;
          break;
        }
        case MSG_QUEUE_MIX: {
          mainMusicQueueMix (mainData, targs);
          dbgdisp = true;
          break;
        }
        case MSG_CMD_PLAY: {
          mainMusicQueuePlay (mainData);
          dbgdisp = true;
          break;
        }
        case MSG_CMD_NEXTSONG: {
          mainNextSong (mainData);
          dbgdisp = true;
          break;
        }
        case MSG_CMD_PLAYPAUSE: {
          if (mainData->playerState == PL_STATE_PLAYING ||
              mainData->playerState == PL_STATE_IN_FADEOUT) {
            connSendMessage (mainData->conn, ROUTE_PLAYER, MSG_PLAY_PAUSE, NULL);
          } else {
            mainMusicQueuePlay (mainData);
          }
          dbgdisp = true;
          break;
        }
        case MSG_PLAYBACK_BEGIN: {
          /* do any begin song processing */
          mainPlaybackBegin (mainData);
          dbgdisp = true;
          break;
        }
        case MSG_PLAYBACK_STOP: {
          mainMusicQueueFinish (mainData, targs);
          dbgdisp = true;
          break;
        }
        case MSG_PLAYBACK_FINISH: {
          mainPlaybackFinishProcess (mainData, targs);
          dbgdisp = true;
          break;
        }
        case MSG_MUSICQ_TOGGLE_PAUSE: {
          mainTogglePause (mainData, targs);
          dbgdisp = true;
          break;
        }
        case MSG_MUSICQ_MOVE_DOWN: {
          mainMusicqMove (mainData, targs, MOVE_DOWN);
          dbgdisp = true;
          break;
        }
        case MSG_MUSICQ_MOVE_TOP: {
          mainMusicqMoveTop (mainData, targs);
          dbgdisp = true;
          break;
        }
        case MSG_MUSICQ_MOVE_UP: {
          mainMusicqMove (mainData, targs, MOVE_UP);
          dbgdisp = true;
          break;
        }
        case MSG_MUSICQ_REMOVE: {
          mainMusicqRemove (mainData, targs);
          dbgdisp = true;
          break;
        }
        case MSG_MUSICQ_TRUNCATE: {
          mainMusicqClear (mainData, targs);
          dbgdisp = true;
          break;
        }
        case MSG_MUSICQ_INSERT: {
          mainMusicqInsert (mainData, routefrom, targs);
          dbgdisp = true;
          break;
        }
        case MSG_MUSICQ_SET_MANAGE: {
          mainMusicqSetManage (mainData, targs);
          dbgdisp = true;
          break;
        }
        case MSG_MUSICQ_SET_PLAYBACK: {
          mainMusicqSetPlayback (mainData, targs);
          dbgdisp = true;
          break;
        }
        case MSG_MUSICQ_SET_LEN: {
          /* a temporary setting used for the song list editor */
          bdjoptSetNum (OPT_G_PLAYERQLEN, atol (targs));
          dbgdisp = true;
          break;
        }
        case MSG_MUSICQ_DATA_SUSPEND: {
          mainMusicqSetSuspend (mainData, targs, true);
          dbgdisp = true;
          break;
        }
        case MSG_MUSICQ_DATA_RESUME: {
          mainMusicqSetSuspend (mainData, targs, false);
          dbgdisp = true;
          break;
        }
        case MSG_PLAYER_STATE: {
          mainData->playerState = (playerstate_t) atol (targs);
          logMsg (LOG_DBG, LOG_MSGS, "got: pl-state: %d/%s",
              mainData->playerState, plstateDebugText (mainData->playerState));
          mainData->marqueeChanged [mainData->musicqPlayIdx] = true;
          if (mainData->playerState == PL_STATE_STOPPED) {
            ++mainData->pbfinishrcv;
          }
          dbgdisp = true;
          break;
        }
        case MSG_GET_DANCE_LIST: {
          mainSendDanceList (mainData, routefrom);
          dbgdisp = true;
          break;
        }
        case MSG_GET_PLAYLIST_LIST: {
          mainSendPlaylistList (mainData, routefrom);
          dbgdisp = true;
          break;
        }
        case MSG_PLAYER_STATUS_DATA: {
          mainSendPlayerStatus (mainData, targs);
          // dbgdisp = true;
          break;
        }
        case MSG_START_MARQUEE: {
          mainStartMarquee (mainData);
          dbgdisp = true;
          break;
        }
        case MSG_DB_ENTRY_UPDATE: {
          dbLoadEntry (mainData->musicdb, atol (targs));
          for (int i = 0; i < MUSICQ_MAX; ++i) {
            mainData->musicqChanged [i] = MAIN_CHG_START;
            mainData->marqueeChanged [i] = true;
          }
          dbgdisp = true;
          break;
        }
        case MSG_DATABASE_UPDATE: {
          mainData->musicdb = bdj4ReloadDatabase (mainData->musicdb);
          musicqSetDatabase (mainData->musicQueue, mainData->musicdb);
          for (int i = 0; i < MUSICQ_MAX; ++i) {
            mainData->musicqChanged [i] = MAIN_CHG_START;
            mainData->marqueeChanged [i] = true;
          }
          dbgdisp = true;
          break;
        }
        case MSG_MAIN_REQ_STATUS: {
          mainStatusRequest (mainData, routefrom);
          dbgdisp = true;
          break;
        }
        case MSG_CHK_MAIN_MUSICQ: {
          mainChkMusicq (mainData, routefrom);
          dbgdisp = true;
          break;
        }
        case MSG_CHK_MAIN_STATUS: {
          dbgdisp = true;
          break;
        }
        case MSG_CHK_MAIN_RESET: {
          mainData->songplaysentcount = 0;
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

  if (dbgdisp) {
    logMsg (LOG_DBG, LOG_MSGS, "got: from:%d/%s route:%d/%s msg:%d/%s args:%s",
        routefrom, msgRouteDebugText (routefrom),
        route, msgRouteDebugText (route), msg, msgDebugText (msg), args);
  }
  if (targs != NULL) {
    free (targs);
  }

  return 0;
}

static int
mainProcessing (void *udata)
{
  maindata_t  *mainData = udata;
  int         stop = false;

  if (! progstateIsRunning (mainData->progstate)) {
    progstateProcess (mainData->progstate);
    if (progstateCurrState (mainData->progstate) == STATE_CLOSED) {
      stop = true;
    }
    if (gKillReceived) {
      progstateShutdownProcess (mainData->progstate);
      logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
    }
    return stop;
  }

  connProcessUnconnected (mainData->conn);

  for (int i = 0; i < MUSICQ_MAX; ++i) {
    if (mainData->changeSuspend [i] == false &&
        mainData->musicqChanged [i] == MAIN_CHG_FINAL) {
      mainSendMusicQueueData (mainData, i);
      mainData->musicqChanged [i] = MAIN_CHG_CLEAR;
    }
    if (mainData->musicqChanged [i] == MAIN_CHG_START) {
      mainData->musicqChanged [i] = MAIN_CHG_FINAL;
    }
  }

  if (mainData->processes [ROUTE_MARQUEE] != NULL &&
      ! connIsConnected (mainData->conn, ROUTE_MARQUEE)) {
    connConnect (mainData->conn, ROUTE_MARQUEE);
  }

  /* for the marquee, only the currently selected playback queue is */
  /* of interest */
  if (mainData->marqueeChanged [mainData->musicqPlayIdx]) {
    mainSendMarqueeData (mainData);
    mainSendMobileMarqueeData (mainData);
    mainData->marqueeChanged [mainData->musicqPlayIdx] = false;
  }

  /* do this after sending the latest musicq / marquee data */
  /* wait for both the PLAYBACK_FINISH message and for the player to stop */
  /* the 'finished' message is sent to the playerui after the player has */
  /* stopped, and it is necessary to wait for after it is sent before */
  /* queueing the next song or playlist */
  if (mainData->waitforpbfinish) {
    if (mainData->pbfinishrcv == 2) {
      if (mainData->pbfinishArgs != NULL) {
        if (mainData->pbfinishType == MAIN_PB_TYPE_PL) {
          mainQueuePlaylist (mainData, mainData->pbfinishArgs);
        }
        if (mainData->pbfinishType == MAIN_PB_TYPE_SONG) {
          mainMusicqInsert (mainData, mainData->pbfinishRoute, mainData->pbfinishArgs);
        }
        free (mainData->pbfinishArgs);
        mainData->pbfinishArgs = NULL;
      }
      mainData->pbfinishrcv = 0;
      mainData->waitforpbfinish = false;
    }
  }

  if (gKillReceived) {
    progstateShutdownProcess (mainData->progstate);
    logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
  }
  return stop;
}

static bool
mainListeningCallback (void *tmaindata, programstate_t programState)
{
  maindata_t    *mainData = tmaindata;
  int           flags;

  logProcBegin (LOG_PROC, "mainListeningCallback");

  flags = PROCUTIL_DETACH;
  if ((mainData->startflags & BDJ4_INIT_NO_DETACH) == BDJ4_INIT_NO_DETACH) {
    flags = PROCUTIL_NO_DETACH;
  }

  if ((mainData->startflags & BDJ4_INIT_NO_START) != BDJ4_INIT_NO_START) {
    char          *script;

    script = bdjoptGetStr (OPT_M_STARTUPSCRIPT);
    if (script != NULL &&
        *script &&
        fileopFileExists (script)) {
      const char  *targv [2];

      targv [0] = script;
      targv [1] = NULL;
      osProcessStart (targv, OS_PROC_DETACH, NULL, NULL);
    }

    mainData->processes [ROUTE_PLAYER] = procutilStartProcess (
        ROUTE_PLAYER, "bdj4player", flags, NULL);
    if (bdjoptGetNum (OPT_P_MOBILEMARQUEE) != MOBILEMQ_OFF) {
      mainData->processes [ROUTE_MOBILEMQ] = procutilStartProcess (
          ROUTE_MOBILEMQ, "bdj4mobmq", flags, NULL);
    }
    if (bdjoptGetNum (OPT_P_REMOTECONTROL)) {
      mainData->processes [ROUTE_REMCTRL] = procutilStartProcess (
          ROUTE_REMCTRL, "bdj4rc", flags, NULL);
    }
  }

  if ((mainData->startflags & BDJ4_INIT_NO_START) != BDJ4_INIT_NO_START &&
      (mainData->startflags & BDJ4_INIT_NO_MARQUEE) != BDJ4_INIT_NO_MARQUEE) {
    mainStartMarquee (mainData);
  }

  logProcEnd (LOG_PROC, "mainListeningCallback", "");
  return STATE_FINISHED;
}

static bool
mainConnectingCallback (void *tmaindata, programstate_t programState)
{
  maindata_t    *mainData = tmaindata;
  int           connMax = 0;
  int           connCount = 0;
  bool          rc = STATE_NOT_FINISH;

  logProcBegin (LOG_PROC, "mainConnectingCallback");

  connProcessUnconnected (mainData->conn);

  if ((mainData->startflags & BDJ4_INIT_NO_START) != BDJ4_INIT_NO_START) {
    if (! connIsConnected (mainData->conn, ROUTE_PLAYER)) {
      connConnect (mainData->conn, ROUTE_PLAYER);
    }
    if (bdjoptGetNum (OPT_P_MOBILEMARQUEE) != MOBILEMQ_OFF) {
      if (! connIsConnected (mainData->conn, ROUTE_MOBILEMQ)) {
        connConnect (mainData->conn, ROUTE_MOBILEMQ);
      }
    }
    if (bdjoptGetNum (OPT_P_REMOTECONTROL)) {
      if (! connIsConnected (mainData->conn, ROUTE_REMCTRL)) {
        connConnect (mainData->conn, ROUTE_REMCTRL);
      }
    }
    if ((mainData->startflags & BDJ4_INIT_NO_MARQUEE) != BDJ4_INIT_NO_MARQUEE) {
      if (! connIsConnected (mainData->conn, ROUTE_MARQUEE)) {
        connConnect (mainData->conn, ROUTE_MARQUEE);
      }
    }
  }

  ++connMax;
  if (connIsConnected (mainData->conn, ROUTE_PLAYER)) {
    ++connCount;
  }
  if (bdjoptGetNum (OPT_P_MOBILEMARQUEE) != MOBILEMQ_OFF) {
    ++connMax;
    if (connIsConnected (mainData->conn, ROUTE_MOBILEMQ)) {
      ++connCount;
    }
  }
  if (bdjoptGetNum (OPT_P_REMOTECONTROL)) {
    ++connMax;
    if (connIsConnected (mainData->conn, ROUTE_REMCTRL)) {
      ++connCount;
    }
  }
  if ((mainData->startflags & BDJ4_INIT_NO_MARQUEE) != BDJ4_INIT_NO_MARQUEE) {
    ++connMax;
    if (connIsConnected (mainData->conn, ROUTE_MARQUEE)) {
      ++connCount;
    }
  }

  if (connCount == connMax) {
    rc = STATE_FINISHED;
  }

  logProcEnd (LOG_PROC, "mainConnectingCallback", "");
  return rc;
}

static bool
mainHandshakeCallback (void *tmaindata, programstate_t programState)
{
  maindata_t    *mainData = tmaindata;
  bool          rc = STATE_NOT_FINISH;
  int           conn = 0;

  logProcBegin (LOG_PROC, "mainHandshakeCallback");

  connProcessUnconnected (mainData->conn);

  if (connHaveHandshake (mainData->conn, ROUTE_STARTERUI)) {
    ++conn;
  }
  if (connHaveHandshake (mainData->conn, ROUTE_PLAYER)) {
    ++conn;
  }
  if (connHaveHandshake (mainData->conn, ROUTE_PLAYERUI) ||
      connHaveHandshake (mainData->conn, ROUTE_MANAGEUI)) {
    ++conn;
  }
  if (connHaveHandshake (mainData->conn, ROUTE_TEST_SUITE)) {
    /* no connection to starterui or playerui/manageui */
    conn += 2;
  }
  /* main must have a connection to the player, starterui, and */
  /* one of manageui and playerui */
  /* alternatively, a connection to the player and the testsuite */
  if (conn == 3) {
    connSendMessage (mainData->conn, ROUTE_PLAYER, MSG_MAIN_READY, NULL);
    rc = STATE_FINISHED;
  }

  logProcEnd (LOG_PROC, "mainHandshakeCallback", "");
  return rc;
}

static void
mainStartMarquee (maindata_t *mainData)
{
  char        *theme;
  const char  *targv [2];
  int         idx = 0;
  int         flags = 0;

  if (mainData->marqueestarted) {
    return;
  }

  /* set the theme for the marquee */
  theme = bdjoptGetStr (OPT_MP_MQ_THEME);
#if BDJ4_USE_GTK
  osSetEnv ("GTK_THEME", theme);
#endif

  if ((mainData->startflags & BDJ4_INIT_HIDE_MARQUEE) == BDJ4_INIT_HIDE_MARQUEE) {
    targv [idx++] = "--hidemarquee";
  }
  targv [idx++] = NULL;

  flags = PROCUTIL_DETACH;
  if ((mainData->startflags & BDJ4_INIT_NO_DETACH) == BDJ4_INIT_NO_DETACH) {
    flags = PROCUTIL_NO_DETACH;
  }

  mainData->processes [ROUTE_MARQUEE] = procutilStartProcess (
      ROUTE_MARQUEE, "bdj4marquee", flags, targv);
  mainData->marqueestarted = true;
}


static void
mainSendMusicQueueData (maindata_t *mainData, int musicqidx)
{
  char        tbuff [200];
  char        sbuff [4100];
  ssize_t     musicqLen;
  slistidx_t  dbidx;
  song_t      *song;
  int         flags;
  int         pflag;
  int         dispidx;
  int         uniqueidx;
  ssize_t     qDuration;

  logProcBegin (LOG_PROC, "mainSendMusicQueueData");

  musicqLen = musicqGetLen (mainData->musicQueue, musicqidx);
  qDuration = musicqGetDuration (mainData->musicQueue, musicqidx);

  sbuff [0] = '\0';
  snprintf (sbuff, sizeof (sbuff), "%d%c%zd%c",
      musicqidx, MSG_ARGS_RS, qDuration, MSG_ARGS_RS);

  for (ssize_t i = 1; i <= musicqLen; ++i) {
    dbidx = musicqGetByIdx (mainData->musicQueue, musicqidx, i);
    song = dbGetByIdx (mainData->musicdb, dbidx);
    if (song != NULL) {
      dispidx = musicqGetDispIdx (mainData->musicQueue, musicqidx, i);
      snprintf (tbuff, sizeof (tbuff), "%d%c", dispidx, MSG_ARGS_RS);
      strlcat (sbuff, tbuff, sizeof (sbuff));
      uniqueidx = musicqGetUniqueIdx (mainData->musicQueue, musicqidx, i);
      snprintf (tbuff, sizeof (tbuff), "%d%c", uniqueidx, MSG_ARGS_RS);
      strlcat (sbuff, tbuff, sizeof (sbuff));
      snprintf (tbuff, sizeof (tbuff), "%d%c", dbidx, MSG_ARGS_RS);
      strlcat (sbuff, tbuff, sizeof (sbuff));
      flags = musicqGetFlags (mainData->musicQueue, musicqidx, i);
      pflag = false;
      if ((flags & MUSICQ_FLAG_PAUSE) == MUSICQ_FLAG_PAUSE) {
        pflag = true;
      }
      snprintf (tbuff, sizeof (tbuff), "%d%c", pflag, MSG_ARGS_RS);
      strlcat (sbuff, tbuff, sizeof (sbuff));
    }
  }

  if (connHaveHandshake (mainData->conn, ROUTE_PLAYERUI)) {
    connSendMessage (mainData->conn, ROUTE_PLAYERUI, MSG_MUSIC_QUEUE_DATA, sbuff);
  }
  if (connHaveHandshake (mainData->conn, ROUTE_MANAGEUI)) {
    connSendMessage (mainData->conn, ROUTE_MANAGEUI, MSG_MUSIC_QUEUE_DATA, sbuff);
  }
  logProcEnd (LOG_PROC, "mainSendMusicQueueData", "");
}

static void
mainSendMarqueeData (maindata_t *mainData)
{
  char        tbuff [200];
  char        sbuff [3096];
  char        *dstr;
  char        *tstr;
  ssize_t     mqLen;
  ssize_t     musicqLen;

  logProcBegin (LOG_PROC, "mainSendMarqueeData");

  if (mainData->playerState == PL_STATE_STOPPED &&
      mainData->finished) {
    logMsg (LOG_DBG, LOG_MAIN, "sending finished");
    mainSendFinished (mainData);
    mainData->finished = false;
    return;
  }

  if (! mainData->marqueestarted) {
    logProcEnd (LOG_PROC, "mainSendMarqueeData", "not-started");
    return;
  }

  mqLen = bdjoptGetNum (OPT_P_MQQLEN);
  musicqLen = musicqGetLen (mainData->musicQueue, mainData->musicqPlayIdx);

  /* artist/title, dance(s) */

  sbuff [0] = '\0';

  dstr = musicqGetData (mainData->musicQueue, mainData->musicqPlayIdx, 0, TAG_ARTIST);
  if (dstr == NULL || *dstr == '\0') { dstr = MSG_ARGS_EMPTY_STR; }
  tstr = musicqGetData (mainData->musicQueue, mainData->musicqPlayIdx, 0, TAG_TITLE);
  if (tstr == NULL || *tstr == '\0') { tstr = MSG_ARGS_EMPTY_STR; }
  snprintf (tbuff, sizeof (tbuff), "%s%c%s%c", dstr, MSG_ARGS_RS, tstr, MSG_ARGS_RS);
  strlcat (sbuff, tbuff, sizeof (sbuff));

  if (musicqLen > 0) {
    for (ssize_t i = 0; i <= mqLen; ++i) {
      if ((i > 0 && mainData->playerState == PL_STATE_IN_GAP) ||
          (i > 1 && mainData->playerState == PL_STATE_IN_FADEOUT)) {
        dstr = MSG_ARGS_EMPTY_STR;
      } else if (i > musicqLen) {
        dstr = MSG_ARGS_EMPTY_STR;
      } else {
        dstr = mainSongGetDanceDisplay (mainData, i);
      }

      snprintf (tbuff, sizeof (tbuff), "%s%c", dstr, MSG_ARGS_RS);
      strlcat (sbuff, tbuff, sizeof (sbuff));
    }
  }

  connSendMessage (mainData->conn, ROUTE_MARQUEE, MSG_MARQUEE_DATA, sbuff);
  logProcEnd (LOG_PROC, "mainSendMarqueeData", "");
}

static char *
mainSongGetDanceDisplay (maindata_t *mainData, ssize_t idx)
{
  char      *tstr;
  char      *dstr;
  dbidx_t   dbidx;
  song_t    *song;

  logProcBegin (LOG_PROC, "mainSongGetDanceDisplay");

  tstr = NULL;
  dbidx = musicqGetByIdx (mainData->musicQueue, mainData->musicqPlayIdx, idx);
  song = dbGetByIdx (mainData->musicdb, dbidx);
  if (song != NULL) {
    /* if the song has an unknown dance, the marquee display */
    /* will be filled in with the dance name. */
    tstr = songGetStr (song, TAG_MQDISPLAY);
  }
  if (tstr != NULL && *tstr) {
    dstr = tstr;
  } else {
    dstr = musicqGetDance (mainData->musicQueue, mainData->musicqPlayIdx, idx);
  }
  if (dstr == NULL) {
    dstr = MSG_ARGS_EMPTY_STR;
  }

  logProcEnd (LOG_PROC, "mainSongGetDanceDisplay", "");
  return dstr;
}


static void
mainSendMobileMarqueeData (maindata_t *mainData)
{
  char        tbuff [200];
  char        tbuffb [200];
  char        qbuff [4096];
  char        jbuff [4096];
  char        *title = NULL;
  char        *dstr = NULL;
  char        *tag = NULL;
  ssize_t     mqLen = 0;
  ssize_t     musicqLen = 0;

  logProcBegin (LOG_PROC, "mainSendMobileMarqueeData");

  if (bdjoptGetNum (OPT_P_MOBILEMARQUEE) == MOBILEMQ_OFF) {
    logProcEnd (LOG_PROC, "mainSendMobileMarqueeData", "is-off");
    return;
  }

  mqLen = bdjoptGetNum (OPT_P_MQQLEN);
  musicqLen = musicqGetLen (mainData->musicQueue, mainData->musicqPlayIdx);
  title = bdjoptGetStr (OPT_P_MOBILEMQTITLE);
  if (title == NULL) {
    title = "";
  }

  strlcpy (jbuff, "{ ", sizeof (jbuff));

  snprintf (tbuff, sizeof (tbuff), "\"mqlen\" : \"%zd\", ", mqLen);
  strlcat (jbuff, tbuff, sizeof (jbuff));
  snprintf (tbuff, sizeof (tbuff), "\"title\" : \"%s\"", title);
  strlcat (jbuff, tbuff, sizeof (jbuff));

  if (musicqLen > 0) {
    for (ssize_t i = 0; i <= mqLen; ++i) {
      if ((i > 0 && mainData->playerState == PL_STATE_IN_GAP) ||
          (i > 1 && mainData->playerState == PL_STATE_IN_FADEOUT)) {
        dstr = "";
      } else if (i > musicqLen) {
        dstr = "";
      } else {
        dstr = mainSongGetDanceDisplay (mainData, i);
        if (dstr == NULL) {
          dstr = "";
        }
      }

      if (i == 0) {
        snprintf (tbuff, sizeof (tbuff), "\"current\" : \"%s\"", dstr);
      } else {
        snprintf (tbuff, sizeof (tbuff), "\"mq%zd\" : \"%s\"", i, dstr);
      }
      strlcat (jbuff, ", ", sizeof (jbuff));
      strlcat (jbuff, tbuff, sizeof (jbuff));
    }
  } else {
    strlcat (jbuff, ", ", sizeof (jbuff));
    strlcat (jbuff, "\"skip\" : \"true\"", sizeof (jbuff));
  }
  strlcat (jbuff, " }", sizeof (jbuff));

  connSendMessage (mainData->conn, ROUTE_MOBILEMQ, MSG_MARQUEE_DATA, jbuff);

  if (bdjoptGetNum (OPT_P_MOBILEMARQUEE) == MOBILEMQ_LOCAL) {
    logProcEnd (LOG_PROC, "mainSendMobileMarqueeData", "is-local");
    return;
  }

  /* internet mode from here on */

  tag = bdjoptGetStr (OPT_P_MOBILEMQTAG);
  if (tag != NULL && *tag != '\0') {
    if (mainData->mobmqUserkey == NULL) {
      pathbldMakePath (tbuff, sizeof (tbuff),
          "mmq", ".key", PATHBLD_MP_DATA | PATHBLD_MP_USEIDX);
      mainData->mobmqUserkey = filedataReadAll (tbuff, NULL);
    }

    snprintf (tbuff, sizeof (tbuff), "%s/%s",
        sysvarsGetStr (SV_HOST_MOBMQ), sysvarsGetStr (SV_URI_MOBMQ_POST));
    snprintf (qbuff, sizeof (qbuff), "v=2&mqdata=%s&key=%s&tag=%s",
        jbuff, "93457645", tag);
    if (mainData->mobmqUserkey != NULL) {
      snprintf (tbuffb, sizeof (tbuffb), "&userkey=%s", mainData->mobmqUserkey);
      strlcat (qbuff, tbuffb, sizeof (qbuff));
    }
    webclientPost (mainData->webclient, tbuff, qbuff);
  }
  logProcEnd (LOG_PROC, "mainSendMobileMarqueeData", "");
}

static void
mainMobilePostCallback (void *userdata, char *resp, size_t len)
{
  maindata_t    *mainData = userdata;
  char          tbuff [MAXPATHLEN];

  logProcBegin (LOG_PROC, "mainMobilePostCallback");

  if (strncmp (resp, "OK", 2) == 0) {
    ;
  } else if (strncmp (resp, "NG", 2) == 0) {
    logMsg (LOG_ERR, LOG_IMPORTANT, "ERR: unable to post mobmq data: %.*s", (int) len, resp);
    /* just turn it off since there is a failure response */
    bdjoptSetNum (OPT_P_MOBILEMARQUEE, MOBILEMQ_OFF);
  } else {
    FILE    *fh;

    /* this should be the user key */
    strlcpy (tbuff, resp, 3);
    tbuff [2] = '\0';
    mainData->mobmqUserkey = strdup (tbuff);
    /* need to save this for future use */
    pathbldMakePath (tbuff, sizeof (tbuff),
        "mmq", ".key", PATHBLD_MP_DATA | PATHBLD_MP_USEIDX);
    fh = fopen (tbuff, "w");
    fprintf (fh, "%.*s", (int) len, resp);
    fclose (fh);
  }
  logProcEnd (LOG_PROC, "mainMobilePostCallback", "");
}

/* clears both the playlist and music queues */
static void
mainQueueClear (maindata_t *mainData, char *args)
{
  int   mi;
  char  *p;
  char  *tokstr = NULL;

  logProcBegin (LOG_PROC, "mainQueueClear");


  p = strtok_r (args, MSG_ARGS_RS_STR, &tokstr);
  mi = atoi (p);

  mainData->musicqManageIdx = mi;

  logMsg (LOG_DBG, LOG_BASIC, "clear music queue");
  queueClear (mainData->playlistQueue [mi], 0);
  musicqClear (mainData->musicQueue, mi, 1);
  mainData->musicqChanged [mi] = MAIN_CHG_START;
  mainData->marqueeChanged [mi] = true;
  logProcEnd (LOG_PROC, "mainQueueClear", "");
}

static void
mainQueueDance (maindata_t *mainData, char *args, ssize_t count)
{
  playlistitem_t  *plitem;
  playlist_t      *playlist;
  int             mi;
  ilistidx_t      danceIdx;
  ilistidx_t      musicqLen;

  logProcBegin (LOG_PROC, "mainQueueDance");

  /* get the musicq length before any songs are added */
  musicqLen = musicqGetLen (mainData->musicQueue, mainData->musicqPlayIdx);

  mainParseIntNum (args, &mi, &danceIdx);
  mainData->musicqManageIdx = mi;

  logMsg (LOG_DBG, LOG_BASIC, "queue dance %d %d %d", mi, danceIdx, count);
  playlist = playlistAlloc (mainData->musicdb);
  /* CONTEXT: player: the name of the special playlist for queueing a dance */
  if (playlistLoad (playlist, _("QueueDance")) < 0) {
    playlistCreate (playlist, "main_queue_dance", PLTYPE_AUTO);
  }
  playlistSetConfigNum (playlist, PLAYLIST_STOP_AFTER, count);
  /* clear all dance selected/counts */
  playlistResetAll (playlist);
  /* this will also set 'selected' */
  playlistSetDanceCount (playlist, danceIdx, 1);
  logMsg (LOG_DBG, LOG_BASIC, "Queue Playlist: %s", "queue-dance");
  plitem = mainPlaylistItemCache (mainData, playlist, globalCounter++);
  queuePush (mainData->playlistQueue [mi], plitem);
  logMsg (LOG_DBG, LOG_MAIN, "push pl %s", "queue-dance");
  mainMusicQueueFill (mainData);
  mainMusicQueuePrep (mainData);
  mainData->marqueeChanged [mi] = true;
  mainData->musicqChanged [mi] = MAIN_CHG_START;
  mainSendMusicqStatus (mainData);
  if (mainData->playWhenQueued &&
      mainData->musicqPlayIdx == (musicqidx_t) mi &&
      mainData->playerState == PL_STATE_STOPPED &&
      musicqLen == 0) {
    mainMusicQueuePlay (mainData);
  }
  logProcEnd (LOG_PROC, "mainQueueDance", "");
}

static void
mainQueuePlaylist (maindata_t *mainData, char *args)
{
  int             mi;
  playlistitem_t  *plitem = NULL;
  playlist_t      *playlist = NULL;
  char            *plname;
  int             rc;
  ilistidx_t      musicqLen;

  logProcBegin (LOG_PROC, "mainQueuePlaylist");

  /* get the musicq length before any songs are added */
  musicqLen = musicqGetLen (mainData->musicQueue, mainData->musicqPlayIdx);

  mainParseIntStr (args, &mi, &plname);
  mainData->musicqManageIdx = mi;

  playlist = playlistAlloc (mainData->musicdb);
  rc = playlistLoad (playlist, plname);

  /* check and see if a stop time override is in effect */
  /* if so, set the playlist's stop time */
  if (mainData->ploverridestoptime > 0) {
    playlistSetConfigNum (playlist, PLAYLIST_STOP_TIME,
        mainData->ploverridestoptime);
  }
  mainData->ploverridestoptime = 0;

  if (rc == 0) {
    logMsg (LOG_DBG, LOG_BASIC, "Queue Playlist: %d %s", mi, plname);
    plitem = mainPlaylistItemCache (mainData, playlist, globalCounter++);
    queuePush (mainData->playlistQueue [mainData->musicqManageIdx], plitem);
    logMsg (LOG_DBG, LOG_MAIN, "push pl %s", plname);
    mainMusicQueueFill (mainData);
    mainMusicQueuePrep (mainData);
    mainData->marqueeChanged [mi] = true;
    mainData->musicqChanged [mi] = MAIN_CHG_START;

    mainSendMusicqStatus (mainData);
    if (mainData->playWhenQueued &&
        mainData->musicqPlayIdx == (musicqidx_t) mi &&
        mainData->playerState == PL_STATE_STOPPED &&
        musicqLen == 0) {
      mainMusicQueuePlay (mainData);
    }
  } else {
    logMsg (LOG_ERR, LOG_IMPORTANT, "ERR: Queue Playlist failed: %s", plname);
  }

  logProcEnd (LOG_PROC, "mainQueuePlaylist", "");
}

static void
mainSigHandler (int sig)
{
  gKillReceived = 1;
}

static void
mainMusicQueueFill (maindata_t *mainData)
{
  ssize_t         playerqLen;
  ssize_t         currlen;
  playlistitem_t  *plitem = NULL;
  playlist_t      *playlist = NULL;
  pltype_t        pltype;
  bool            stopatflag = false;
  long            dur;

  logProcBegin (LOG_PROC, "mainMusicQueueFill");

  plitem = queueGetCurrent (mainData->playlistQueue [mainData->musicqManageIdx]);
  playlist = NULL;
  if (plitem != NULL) {
    playlist = plitem->playlist;
  }
  pltype = (pltype_t) playlistGetConfigNum (playlist, PLAYLIST_TYPE);

  playerqLen = bdjoptGetNum (OPT_G_PLAYERQLEN);
  currlen = musicqGetLen (mainData->musicQueue, mainData->musicqManageIdx);

  /* if this is not the queue selected for playback, then push  */
  /* an empty musicq item on to the head of the queue.          */
  /* this allows the display to work properly for the user      */
  /* when multiple queues are in use.                           */

  if (currlen == 0 &&
      mainData->musicqPlayIdx != mainData->musicqManageIdx) {
    musicqPushHeadEmpty (mainData->musicQueue, mainData->musicqManageIdx);
    ++currlen;
  }

  logMsg (LOG_DBG, LOG_BASIC, "fill: %ld < %ld", currlen, playerqLen);

  /* want current + playerqLen songs */
  while (playlist != NULL && currlen <= playerqLen && stopatflag == false) {
    time_t  stopTime;
    song_t  *song = NULL;

    song = playlistGetNextSong (playlist, mainData->danceCounts,
        currlen, mainMusicQueueHistory, mainData);
    if (song == NULL) {
      logMsg (LOG_DBG, LOG_MAIN, "song is null");
      plitem = queuePop (mainData->playlistQueue [mainData->musicqManageIdx]);
      mainPlaylistItemFree (plitem);
      plitem = queueGetCurrent (mainData->playlistQueue [mainData->musicqManageIdx]);
      playlist = NULL;
      if (plitem != NULL) {
        playlist = plitem->playlist;
      }
      continue;
    }

    logMsg (LOG_DBG, LOG_MAIN, "push song to musicq");
    dur = mainCalculateSongDuration (mainData, song, plitem->playlistIdx);
    musicqPush (mainData->musicQueue, mainData->musicqManageIdx,
        songGetNum (song, TAG_DBIDX), plitem->playlistIdx, dur);
    mainData->musicqChanged [mainData->musicqManageIdx] = MAIN_CHG_START;
    mainData->marqueeChanged [mainData->musicqManageIdx] = true;
    currlen = musicqGetLen (mainData->musicQueue, mainData->musicqManageIdx);

    if (pltype == PLTYPE_AUTO) {
      plitem = queueGetCurrent (mainData->playlistQueue [mainData->musicqManageIdx]);
      playlist = NULL;
      if (plitem != NULL) {
        playlist = plitem->playlist;
      }
      if (playlist != NULL && song != NULL) {
        playlistAddCount (playlist, song);
      }
    }

    stopTime = playlistGetConfigNum (playlist, PLAYLIST_STOP_TIME);
    if (stopTime > 0) {
      time_t  currTime;
      time_t  qDuration;
      time_t  nStopTime;

      currTime = mstime ();
      /* stop time is in hours+minutes; need to convert it to real time */
      nStopTime = mstimestartofday ();
      nStopTime += stopTime;
      if (nStopTime < currTime) {
        nStopTime += (24 * 3600 * 1000);
      }

      qDuration = musicqGetDuration (mainData->musicQueue, mainData->musicqManageIdx);
      if (currTime + qDuration > nStopTime) {
        stopatflag = true;
      }
    }
  }

  logProcEnd (LOG_PROC, "mainMusicQueueFill", "");
}

static void
mainMusicQueuePrep (maindata_t *mainData)
{
  playlist_t    *playlist;
  ssize_t       plannounce;

  logProcBegin (LOG_PROC, "mainMusicQueuePrep");

  /* 5 is the number of songs to prep ahead of time */
  for (ssize_t i = 0; i < MAIN_PREP_SIZE; ++i) {
    char          *sfname = NULL;
    dbidx_t       dbidx;
    song_t        *song = NULL;
    musicqflag_t  flags;
    char          *annfname = NULL;
    int           playlistIdx;

    dbidx = musicqGetByIdx (mainData->musicQueue, mainData->musicqManageIdx, i);
    song = dbGetByIdx (mainData->musicdb, dbidx);
    flags = musicqGetFlags (mainData->musicQueue, mainData->musicqManageIdx, i);
    playlistIdx = musicqGetPlaylistIdx (mainData->musicQueue, mainData->musicqManageIdx, i);

    if (song != NULL &&
        (flags & MUSICQ_FLAG_PREP) != MUSICQ_FLAG_PREP) {
      musicqSetFlag (mainData->musicQueue, mainData->musicqManageIdx,
          i, MUSICQ_FLAG_PREP);

      plannounce = false;
      if (playlistIdx != -1) {
        playlist = nlistGetData (mainData->playlistCache, playlistIdx);
        plannounce = playlistGetConfigNum (playlist, PLAYLIST_ANNOUNCE);
      }
      annfname = mainPrepSong (mainData, song, sfname, playlistIdx, PREP_SONG);

      if (plannounce == 1) {
        if (annfname != NULL && strcmp (annfname, "") != 0 ) {
          musicqSetFlag (mainData->musicQueue, mainData->musicqManageIdx,
              i, MUSICQ_FLAG_ANNOUNCE);
          musicqSetAnnounce (mainData->musicQueue, mainData->musicqManageIdx,
              i, annfname);
        }
      }
    }
  }
  logProcEnd (LOG_PROC, "mainMusicQueuePrep", "");
}


static char *
mainPrepSong (maindata_t *mainData, song_t *song,
    char *sfname, int playlistIdx, bdjmsgprep_t flag)
{
  char          tbuff [MAXPATHLEN];
  playlist_t    *playlist = NULL;
  ssize_t       dur = 0;
  ssize_t       plgap = -1;
  ssize_t       songstart = 0;
  ssize_t       speed = 100;
  double        voladjperc = 0;
  ssize_t       gap = 0;
  ssize_t       plannounce = 0;
  ilistidx_t    danceidx;
  char          *annfname = NULL;

  logProcBegin (LOG_PROC, "mainPrepSong");

  sfname = songGetStr (song, TAG_FILE);
  voladjperc = songGetDouble (song, TAG_VOLUMEADJUSTPERC);
  if (voladjperc == LIST_DOUBLE_INVALID) {
    voladjperc = 0.0;
  }
  gap = 0;

  songstart = songGetNum (song, TAG_SONGSTART);
  if (songstart < 0) { songstart = 0; }
  dur = songGetNum (song, TAG_DURATION);

  speed = songGetNum (song, TAG_SPEEDADJUSTMENT);
  if (speed < 0) {
    speed = 100;
  }

  /* announcements don't need any of the following... */
  if (flag != PREP_ANNOUNCE) {
    dur = mainCalculateSongDuration (mainData, song, playlistIdx);

    gap = mainData->gap;
    plgap = playlistGetConfigNum (playlist, PLAYLIST_GAP);
    if (plgap >= 0) {
      gap = plgap;
    }

    plannounce = playlistGetConfigNum (playlist, PLAYLIST_ANNOUNCE);
    if (plannounce == 1) {
      dance_t       *dances;
      song_t        *tsong;

      danceidx = songGetNum (song, TAG_DANCE);
      dances = bdjvarsdfGet (BDJVDF_DANCES);
      annfname = danceGetStr (dances, danceidx, DANCE_ANNOUNCE);
      if (annfname != NULL) {
        ssize_t   tval;

        tsong = dbGetByName (mainData->musicdb, annfname);
        if (tsong != NULL) {
          tval = slistGetNum (mainData->announceList, annfname);
          if (tval == LIST_VALUE_INVALID) {
            /* only prep the announcement if it has not been prepped before */
            mainPrepSong (mainData, tsong, annfname, playlistIdx, PREP_ANNOUNCE);
          }
          slistSetNum (mainData->announceList, annfname, 1);
        } else {
          annfname = NULL;
        }
      } /* found an announcement for the dance */
    } /* announcements are on in the playlist */
  } /* if this is a normal song */

  snprintf (tbuff, sizeof (tbuff), "%s%c%zd%c%zd%c%zd%c%.1f%c%zd%c%d",
      sfname, MSG_ARGS_RS,
      dur, MSG_ARGS_RS,
      songstart, MSG_ARGS_RS,
      speed, MSG_ARGS_RS,
      voladjperc, MSG_ARGS_RS,
      gap, MSG_ARGS_RS,
      flag);

  logMsg (LOG_DBG, LOG_MAIN, "prep song %s", sfname);
  connSendMessage (mainData->conn, ROUTE_PLAYER, MSG_SONG_PREP, tbuff);
  logProcEnd (LOG_PROC, "mainPrepSong", "");
  return annfname;
}

static void
mainTogglePause (maindata_t *mainData, char *args)
{
  int           mi;
  ilistidx_t    idx;
  ilistidx_t    musicqLen;
  musicqflag_t  flags;

  logProcBegin (LOG_PROC, "mainTogglePause");

  mainParseIntNum (args, &mi, &idx);
  mainData->musicqManageIdx = mi;

  musicqLen = musicqGetLen (mainData->musicQueue, mainData->musicqManageIdx);
  if (idx <= 0 || idx > musicqLen) {
    logProcEnd (LOG_PROC, "mainTogglePause", "bad-idx");
    return;
  }

  flags = musicqGetFlags (mainData->musicQueue, mainData->musicqManageIdx, idx);
  if ((flags & MUSICQ_FLAG_PAUSE) == MUSICQ_FLAG_PAUSE) {
    musicqClearFlag (mainData->musicQueue, mainData->musicqManageIdx, idx, MUSICQ_FLAG_PAUSE);
  } else {
    musicqSetFlag (mainData->musicQueue, mainData->musicqManageIdx, idx, MUSICQ_FLAG_PAUSE);
  }
  mainData->musicqChanged [mi] = MAIN_CHG_START;
  logProcEnd (LOG_PROC, "mainTogglePause", "");
}

static void
mainMusicqMove (maindata_t *mainData, char *args, mainmove_t direction)
{
  int           mi;
  ilistidx_t    fromidx;
  ilistidx_t    toidx;
  ilistidx_t    musicqLen;

  logProcBegin (LOG_PROC, "mainMusicqMove");


  mainParseIntNum (args, &mi, &fromidx);
  mainData->musicqManageIdx = mi;

  musicqLen = musicqGetLen (mainData->musicQueue, mainData->musicqManageIdx);

  toidx = fromidx + direction;
  if (fromidx == 1 && toidx == 0 &&
      mainData->playerState != PL_STATE_STOPPED) {
    logProcEnd (LOG_PROC, "mainMusicqMove", "to-0-playing");
    return;
  }

  if (toidx > fromidx && fromidx >= musicqLen) {
    logProcEnd (LOG_PROC, "mainMusicqMove", "bad-move");
    return;
  }
  if (toidx < fromidx && fromidx < 1) {
    logProcEnd (LOG_PROC, "mainMusicqMove", "bad-move");
    return;
  }

  musicqMove (mainData->musicQueue, mainData->musicqManageIdx, fromidx, toidx);
  mainData->musicqChanged [mi] = MAIN_CHG_START;
  mainData->marqueeChanged [mi] = true;
  mainMusicQueuePrep (mainData);

  if (toidx == 0) {
    mainSendMusicqStatus (mainData);
  }
  logProcEnd (LOG_PROC, "mainMusicqMove", "");
}

static void
mainMusicqMoveTop (maindata_t *mainData, char *args)
{
  int         mi;
  ilistidx_t  fromidx;
  ilistidx_t  toidx;
  ilistidx_t  musicqLen;

  logProcBegin (LOG_PROC, "mainMusicqMoveTop");

  mainParseIntNum (args, &mi, &fromidx);
  mainData->musicqManageIdx = mi;

  musicqLen = musicqGetLen (mainData->musicQueue, mainData->musicqManageIdx);

  if (fromidx >= musicqLen || fromidx <= 1) {
    logProcEnd (LOG_PROC, "mainMusicqMoveTop", "bad-move");
    return;
  }

  toidx = fromidx - 1;
  while (toidx != 0) {
    musicqMove (mainData->musicQueue, mainData->musicqManageIdx, fromidx, toidx);
    fromidx--;
    toidx--;
  }
  mainData->musicqChanged [mi] = MAIN_CHG_START;
  mainData->marqueeChanged [mi] = true;
  mainMusicQueuePrep (mainData);
  logProcEnd (LOG_PROC, "mainMusicqMoveTop", "");
}

static void
mainMusicqClear (maindata_t *mainData, char *args)
{
  int         mi;
  ilistidx_t  idx;

  logProcBegin (LOG_PROC, "mainMusicqClear");

  mainParseIntNum (args, &mi, &idx);
  mainData->musicqManageIdx = mi;

  musicqClear (mainData->musicQueue, mainData->musicqManageIdx, idx);
  /* there may be other playlists in the playlist queue */
  mainMusicQueueFill (mainData);
  mainMusicQueuePrep (mainData);
  mainData->musicqChanged [mi] = MAIN_CHG_START;
  mainData->marqueeChanged [mi] = true;
  logProcEnd (LOG_PROC, "mainMusicqClear", "");
}

static void
mainMusicqRemove (maindata_t *mainData, char *args)
{
  int         mi;
  ilistidx_t  idx;

  logProcBegin (LOG_PROC, "mainMusicqRemove");


  mainParseIntNum (args, &mi, &idx);
  mainData->musicqManageIdx = mi;

  musicqRemove (mainData->musicQueue, mainData->musicqManageIdx, idx);
  mainMusicQueueFill (mainData);
  mainMusicQueuePrep (mainData);
  mainData->musicqChanged [mi] = MAIN_CHG_START;
  mainData->marqueeChanged [mi] = true;
  logProcEnd (LOG_PROC, "mainMusicqRemove", "");
}

static void
mainNextSong (maindata_t *mainData)
{
  int   currlen;

  currlen = musicqGetLen (mainData->musicQueue, mainData->musicqPlayIdx);
  if (currlen > 0) {
    connSendMessage (mainData->conn, ROUTE_PLAYER, MSG_PLAY_NEXTSONG, NULL);
    mainData->waitforpbfinish = true;
    mainData->pbfinishrcv = 0;
  }
}

static void
mainMusicqInsert (maindata_t *mainData, bdjmsgroute_t routefrom, char *args)
{
  int       mi;
  char      *tokstr = NULL;
  char      *p = NULL;
  long      idx;
  dbidx_t   dbidx;
  song_t    *song = NULL;
  long      currlen;


  logProcBegin (LOG_PROC, "mainMusicqInsert");

  p = strtok_r (args, MSG_ARGS_RS_STR, &tokstr);
  if (p == NULL) {
    logProcEnd (LOG_PROC, "mainMusicqInsert", "parse-fail-a");
    return;
  }
  mi = atoi (p);
  if (mi != MUSICQ_CURRENT) {
    /* the managed queue is changed to the requested queue */
    mainData->musicqManageIdx = mi;
  }
  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  if (p == NULL) {
    logProcEnd (LOG_PROC, "mainMusicqInsert", "parse-fail-b");
    return;
  }
  idx = atol (p);
  /* the display selection is offset by 1 */
  /* and want to insert after the selection */
  idx += 2;
  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  if (p == NULL) {
    logProcEnd (LOG_PROC, "mainMusicqInsert", "parse-fail-c");
    return;
  }
  dbidx = atol (p);

  song = dbGetByIdx (mainData->musicdb, dbidx);

  currlen = musicqGetLen (mainData->musicQueue, mainData->musicqManageIdx);
  if (currlen == 0 &&
      mainData->musicqPlayIdx != mainData->musicqManageIdx) {
    musicqPushHeadEmpty (mainData->musicQueue, mainData->musicqManageIdx);
  }

  if (song != NULL) {
    long  loc;
    long  dur;

    dur = mainCalculateSongDuration (mainData, song, -1);
    loc = musicqInsert (mainData->musicQueue, mainData->musicqManageIdx,
        idx, dbidx, dur);
    mainData->musicqChanged [mainData->musicqManageIdx] = MAIN_CHG_START;
    mainData->marqueeChanged [mainData->musicqManageIdx] = true;
    mainMusicQueuePrep (mainData);
    mainSendMusicqStatus (mainData);
    if (mainData->playerState == PL_STATE_STOPPED &&
        mainData->playWhenQueued &&
        mainData->musicqPlayIdx == mainData->musicqManageIdx) {
      mainMusicQueuePlay (mainData);
    }
    if (loc > 0) {
      char  tbuff [40];

      snprintf (tbuff, sizeof (tbuff), "%d%c%ld",
          mainData->musicqManageIdx, MSG_ARGS_RS, loc);
      connSendMessage (mainData->conn, routefrom, MSG_SONG_SELECT, tbuff);
    }
  }
  logProcEnd (LOG_PROC, "mainMusicqInsert", "");
}

static void
mainMusicqSetManage (maindata_t *mainData, char *args)
{
  musicqidx_t   mqidx;

  logProcBegin (LOG_PROC, "mainMusicqSetManage");

  mqidx = atoi (args);
  if (mqidx < 0 || mqidx >= MUSICQ_MAX) {
    logProcEnd (LOG_PROC, "mainMusicqSetManage", "bad-idx");
    return;
  }

  mainData->musicqManageIdx = mqidx;
  logProcEnd (LOG_PROC, "mainMusicqSetManage", "");
}

static void
mainMusicqSetPlayback (maindata_t *mainData, char *args)
{
  musicqidx_t   mqidx;

  logProcBegin (LOG_PROC, "mainMusicqSetPlayback");

  mqidx = atoi (args);
  if (mqidx < 0 || mqidx >= MUSICQ_MAX) {
    logProcEnd (LOG_PROC, "mainMusicqSetPlayback", "bad-idx");
    return;
  }

  if (mqidx == mainData->musicqPlayIdx) {
    mainData->musicqDeferredPlayIdx = MAIN_NOT_SET;
  } else if (mainData->playerState != PL_STATE_STOPPED) {
    /* if the player is playing something, the musicq index cannot */
    /* be changed as yet */
    mainData->musicqDeferredPlayIdx = mqidx;
  } else {
    mainMusicqSwitch (mainData, mqidx);
  }
  logProcEnd (LOG_PROC, "mainMusicqSetPlayback", "");
}

static void
mainMusicqSwitch (maindata_t *mainData, musicqidx_t newMusicqIdx)
{
  dbidx_t       dbidx;
  song_t        *song;
  musicqidx_t   oldplayidx;

  logProcBegin (LOG_PROC, "mainMusicqSwitch");

  oldplayidx = mainData->musicqPlayIdx;
  mainData->musicqManageIdx = oldplayidx;
  /* manage idx is pointing to the previous musicq play idx */
  musicqPushHeadEmpty (mainData->musicQueue, mainData->musicqManageIdx);
  /* the prior musicq has changed */
  mainData->musicqChanged [mainData->musicqManageIdx] = MAIN_CHG_START;
  /* send the prior musicq update while the play idx is still set */
  mainSendMusicqStatus (mainData);

  /* now change the play idx */
  mainData->musicqPlayIdx = newMusicqIdx;
  mainData->musicqManageIdx = mainData->musicqPlayIdx;
  mainData->musicqDeferredPlayIdx = MAIN_NOT_SET;

  dbidx = musicqGetCurrent (mainData->musicQueue, mainData->musicqPlayIdx);
  song = dbGetByIdx (mainData->musicdb, dbidx);
  if (song == NULL) {
    musicqPop (mainData->musicQueue, mainData->musicqPlayIdx);
  }

  /* and now send the musicq update for the new play idx */
  mainSendMusicqStatus (mainData);
  mainData->musicqChanged [mainData->musicqPlayIdx] = MAIN_CHG_START;
  mainData->marqueeChanged [mainData->musicqPlayIdx] = true;
  logProcEnd (LOG_PROC, "mainMusicqSwitch", "");
}

static void
mainPlaybackBegin (maindata_t *mainData)
{
  musicqflag_t  flags;

  logProcBegin (LOG_PROC, "mainPlaybackBegin");

  /* if the pause flag is on for this entry in the music queue, */
  /* tell the player to turn on the pause-at-end */
  flags = musicqGetFlags (mainData->musicQueue, mainData->musicqPlayIdx, 0);
  if ((flags & MUSICQ_FLAG_PAUSE) == MUSICQ_FLAG_PAUSE) {
    connSendMessage (mainData->conn, ROUTE_PLAYER,
        MSG_PLAY_PAUSEATEND, NULL);
  }
  logProcEnd (LOG_PROC, "mainPlaybackBegin", "");
}

static void
mainMusicQueuePlay (maindata_t *mainData)
{
  musicqflag_t      flags;
  char              *sfname;
  dbidx_t           dbidx;
  song_t            *song;
  int               currlen;
  musicqidx_t       origMusicqPlayIdx;

  logProcBegin (LOG_PROC, "mainMusicQueuePlay");

  if (mainData->musicqDeferredPlayIdx != MAIN_NOT_SET) {
    mainMusicqSwitch (mainData, mainData->musicqDeferredPlayIdx);
    mainData->musicqDeferredPlayIdx = MAIN_NOT_SET;
  }

  mainData->musicqManageIdx = mainData->musicqPlayIdx;

  logMsg (LOG_DBG, LOG_BASIC, "pl-state: %d/%s",
      mainData->playerState, plstateDebugText (mainData->playerState));

  if (mainData->playerState != PL_STATE_PAUSED) {
    /* grab a song out of the music queue and start playing */
    logMsg (LOG_DBG, LOG_MAIN, "player sent a finish; get song, start");
    dbidx = musicqGetCurrent (mainData->musicQueue, mainData->musicqPlayIdx);
    song = dbGetByIdx (mainData->musicdb, dbidx);
    if (song != NULL) {
      flags = musicqGetFlags (mainData->musicQueue, mainData->musicqPlayIdx, 0);
      if ((flags & MUSICQ_FLAG_ANNOUNCE) == MUSICQ_FLAG_ANNOUNCE) {
        char      *annfname;

        annfname = musicqGetAnnounce (mainData->musicQueue, mainData->musicqPlayIdx, 0);
        if (annfname != NULL) {
          connSendMessage (mainData->conn, ROUTE_PLAYER, MSG_SONG_PLAY, annfname);
        }
      }
      sfname = songGetStr (song, TAG_FILE);
      connSendMessage (mainData->conn, ROUTE_PLAYER, MSG_SONG_PLAY, sfname);
      ++mainData->songplaysentcount;
    }

    currlen = musicqGetLen (mainData->musicQueue, mainData->musicqPlayIdx);
    origMusicqPlayIdx = mainData->musicqPlayIdx;

    if (song == NULL && currlen == 0) {
      logMsg (LOG_DBG, LOG_MAIN, "no songs left in queue");
      if (mainData->switchQueueWhenEmpty) {
        char    tmp [40];

        logMsg (LOG_DBG, LOG_MAIN, "switch queues");
        mainData->musicqPlayIdx = musicqNextQueue (mainData->musicqPlayIdx);
        mainData->musicqManageIdx = mainData->musicqPlayIdx;
        currlen = musicqGetLen (mainData->musicQueue, mainData->musicqPlayIdx);

        /* locate a queue that has songs in it */
        while (mainData->musicqPlayIdx != origMusicqPlayIdx &&
            currlen == 0) {
          mainData->musicqPlayIdx = musicqNextQueue (mainData->musicqPlayIdx);
          mainData->musicqManageIdx = mainData->musicqPlayIdx;
          currlen = musicqGetLen (mainData->musicQueue, mainData->musicqPlayIdx);
        }

        if (currlen > 0) {
          snprintf (tmp, sizeof (tmp), "%d", mainData->musicqPlayIdx);
          connSendMessage (mainData->conn, ROUTE_PLAYERUI, MSG_QUEUE_SWITCH, tmp);
          /* and start up playback for the new queue */
          /* the next-song flag is always 0 here */
          mainMusicQueueNext (mainData, "0");
          /* since the player state is stopped, must re-start playback */
          mainMusicQueuePlay (mainData);
        } else {
          /* there is no music to play; tell the player to clear its display */
          logMsg (LOG_DBG, LOG_MAIN, "sqwe:true no music to play: finished <= true");
          mainData->finished = true;
          mainData->marqueeChanged [mainData->musicqPlayIdx] = true;
        }
      } else {
        logMsg (LOG_DBG, LOG_MAIN, "no more songs; pl-state: %d/%s; finished <= true",
            mainData->playerState, plstateDebugText (mainData->playerState));
        mainData->finished = true;
        mainData->marqueeChanged [mainData->musicqPlayIdx] = true;
      }
    } /* if the song was null and the queue is empty */
  } /* song is not paused */

  /* this handles the user-selected play button when the song is paused */
  /* it no longer handles in-gap due to some changes */
  /* this might need to be moved to the message handler */
  if (mainData->playerState == PL_STATE_PAUSED) {
    logMsg (LOG_DBG, LOG_MAIN, "player is paused, send play msg");
    connSendMessage (mainData->conn, ROUTE_PLAYER, MSG_PLAY_PLAY, NULL);
  }

  logProcEnd (LOG_PROC, "mainMusicQueuePlay", "");
}

static void
mainMusicQueueFinish (maindata_t *mainData, const char *args)
{
  playlist_t    *playlist;
  dbidx_t       dbidx;
  song_t        *song;
  ilistidx_t    danceIdx;
  int           playlistIdx;

  logProcBegin (LOG_PROC, "mainMusicQueueFinish");

  mainData->musicqManageIdx = mainData->musicqPlayIdx;

  mainPlaybackSendSongFinish (mainData, args);

  /* let the playlist know this song has been played */
  dbidx = musicqGetCurrent (mainData->musicQueue, mainData->musicqPlayIdx);
  song = dbGetByIdx (mainData->musicdb, dbidx);
  playlistIdx = musicqGetPlaylistIdx (mainData->musicQueue, mainData->musicqPlayIdx, 0);
  if (playlistIdx != -1) {
    playlist = nlistGetData (mainData->playlistCache, playlistIdx);
    if (playlist != NULL && song != NULL) {
      playlistAddPlayed (playlist, song);
    }
  }
  /* update the dance counts */
  if (song != NULL) {
    danceIdx = songGetNum (song, TAG_DANCE);
    if (danceIdx >= 0) {
      nlistDecrement (mainData->danceCounts, danceIdx);
    }
  }

  musicqPop (mainData->musicQueue, mainData->musicqPlayIdx);
  mainData->musicqChanged [mainData->musicqPlayIdx] = MAIN_CHG_START;
  mainData->marqueeChanged [mainData->musicqPlayIdx] = true;
  mainSendMusicqStatus (mainData);
  logProcEnd (LOG_PROC, "mainMusicQueueFinish", "");
}

static void
mainMusicQueueNext (maindata_t *mainData, const char *args)
{
  logProcBegin (LOG_PROC, "mainMusicQueueNext");

  mainData->musicqManageIdx = mainData->musicqPlayIdx;

  mainMusicQueueFinish (mainData, args);
  if (mainData->playerState != PL_STATE_STOPPED &&
      mainData->playerState != PL_STATE_PAUSED) {
    mainMusicQueuePlay (mainData);
  }
  mainMusicQueueFill (mainData);
  mainMusicQueuePrep (mainData);

  if (mainData->playerState == PL_STATE_STOPPED ||
      mainData->playerState == PL_STATE_PAUSED) {
    if (musicqGetLen (mainData->musicQueue, mainData->musicqPlayIdx) == 0) {
      mainData->finished = true;
    }
  }
  logProcEnd (LOG_PROC, "mainMusicQueueNext", "");
}

/* this is terribly inefficient */
/* for the time being, it will be left as is until such time */
/* that the structure of 'dancesel' can be reviewed and possibly */
/* reworked */
static ilistidx_t
mainMusicQueueHistory (void *tmaindata, ilistidx_t idx)
{
  maindata_t    *mainData = tmaindata;
  ilistidx_t    didx = -1;
  dbidx_t       dbidx;
  song_t        *song;

  logProcBegin (LOG_PROC, "mainMusicQueueHistory");

  if (idx < 0 ||
      idx >= musicqGetLen (mainData->musicQueue, mainData->musicqManageIdx)) {
    logProcEnd (LOG_PROC, "mainMusicQueueHistory", "bad-idx");
    return -1;
  }

  dbidx = musicqGetByIdx (mainData->musicQueue, mainData->musicqManageIdx, idx);
  song = dbGetByIdx (mainData->musicdb, dbidx);
  if (song != NULL) {
    didx = songGetNum (song, TAG_DANCE);
  }
  logProcEnd (LOG_PROC, "mainMusicQueueHistory", "");
  return didx;
}

static void
mainSendDanceList (maindata_t *mainData, bdjmsgroute_t route)
{
  dance_t       *dances;
  slist_t       *danceList;
  slistidx_t    idx;
  char          *dancenm;
  char          tbuff [200];
  char          rbuff [3096];
  slistidx_t    iteridx;

  logProcBegin (LOG_PROC, "mainSendDanceList");

  dances = bdjvarsdfGet (BDJVDF_DANCES);
  danceList = danceGetDanceList (dances);

  rbuff [0] = '\0';
  slistStartIterator (danceList, &iteridx);
  while ((dancenm = slistIterateKey (danceList, &iteridx)) != NULL) {
    idx = slistGetNum (danceList, dancenm);
    snprintf (tbuff, sizeof (tbuff), "%d%c%s%c",
        idx, MSG_ARGS_RS, dancenm, MSG_ARGS_RS);
    strlcat (rbuff, tbuff, sizeof (rbuff));
  }

  connSendMessage (mainData->conn, route, MSG_DANCE_LIST_DATA, rbuff);
  logProcEnd (LOG_PROC, "mainSendDanceList", "");
}

static void
mainSendPlaylistList (maindata_t *mainData, bdjmsgroute_t route)
{
  slist_t       *plList;
  char          *plfnm;
  char          *plnm;
  char          tbuff [200];
  char          rbuff [3096];
  slistidx_t    iteridx;

  logProcBegin (LOG_PROC, "mainSendPlaylistList");

  plList = playlistGetPlaylistList (PL_LIST_NORMAL);

  rbuff [0] = '\0';
  slistStartIterator (plList, &iteridx);
  while ((plnm = slistIterateKey (plList, &iteridx)) != NULL) {
    plfnm = slistGetStr (plList, plnm);
    snprintf (tbuff, sizeof (tbuff), "%s%c%s%c",
        plfnm, MSG_ARGS_RS, plnm, MSG_ARGS_RS);
    strlcat (rbuff, tbuff, sizeof (rbuff));
  }

  slistFree (plList);

  connSendMessage (mainData->conn, route, MSG_PLAYLIST_LIST_DATA, rbuff);
  logProcEnd (LOG_PROC, "mainSendPlaylistList", "");
}

static void
mainSendPlayerStatus (maindata_t *mainData, char *playerResp)
{
  char    tbuff [200];
  char    tbuff2 [40];
  char    jsbuff [3096];
  char    timerbuff [1024];
  char    statusbuff [1024];
  char    *tokstr = NULL;
  char    *p;
  int     jsonflag;
  int     musicqLen;

  logProcBegin (LOG_PROC, "mainSendPlayerStatus");

  jsonflag = bdjoptGetNum (OPT_P_REMOTECONTROL);
  statusbuff [0] = '\0';

  if (jsonflag) {
    strlcpy (jsbuff, "{ ", sizeof (jsbuff));
  }

  p = strtok_r (playerResp, MSG_ARGS_RS_STR, &tokstr);
  if (jsonflag) {
    snprintf (tbuff, sizeof (tbuff),
        "\"playstate\" : \"%s\"", p);
    strlcat (jsbuff, tbuff, sizeof (jsbuff));
  }
  snprintf (tbuff, sizeof (tbuff), "%s%c", p, MSG_ARGS_RS);
  strlcat (statusbuff, tbuff, sizeof (statusbuff));

  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  if (jsonflag) {
    snprintf (tbuff, sizeof (tbuff),
        "\"repeat\" : \"%s\"", p);
    strlcat (jsbuff, ", ", sizeof (jsbuff));
    strlcat (jsbuff, tbuff, sizeof (jsbuff));
  }
  snprintf (tbuff, sizeof (tbuff), "%s%c", p, MSG_ARGS_RS);
  strlcat (statusbuff, tbuff, sizeof (statusbuff));

  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  if (jsonflag) {
    snprintf (tbuff, sizeof (tbuff),
        "\"pauseatend\" : \"%s\"", p);
    strlcat (jsbuff, ", ", sizeof (jsbuff));
    strlcat (jsbuff, tbuff, sizeof (jsbuff));
  }
  snprintf (tbuff, sizeof (tbuff), "%s%c", p, MSG_ARGS_RS);
  strlcat (statusbuff, tbuff, sizeof (statusbuff));

  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  if (jsonflag) {
    snprintf (tbuff, sizeof (tbuff),
        "\"vol\" : \"%s%%\"", p);
    strlcat (jsbuff, ", ", sizeof (jsbuff));
    strlcat (jsbuff, tbuff, sizeof (jsbuff));
  }
  snprintf (tbuff, sizeof (tbuff), "%s%c", p, MSG_ARGS_RS);
  strlcat (statusbuff, tbuff, sizeof (statusbuff));

  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  if (jsonflag) {
    snprintf (tbuff, sizeof (tbuff),
        "\"speed\" : \"%s%%\"", p);
    strlcat (jsbuff, ", ", sizeof (jsbuff));
    strlcat (jsbuff, tbuff, sizeof (jsbuff));
  }
  snprintf (tbuff, sizeof (tbuff), "%s%c", p, MSG_ARGS_RS);
  strlcat (statusbuff, tbuff, sizeof (statusbuff));

  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  if (jsonflag) {
    snprintf (tbuff, sizeof (tbuff),
        "\"playedtime\" : \"%s\"", tmutilToMS (atol (p), tbuff2, sizeof (tbuff2)));
    strlcat (jsbuff, ", ", sizeof (jsbuff));
    strlcat (jsbuff, tbuff, sizeof (jsbuff));
  }

  /* for marquee */
  snprintf (tbuff, sizeof (tbuff), "%s%c", p, MSG_ARGS_RS);
  strlcpy (timerbuff, tbuff, sizeof (timerbuff));

  /* for player */
  snprintf (tbuff, sizeof (tbuff), "%s%c", p, MSG_ARGS_RS);
  strlcat (statusbuff, tbuff, sizeof (statusbuff));

  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  if (jsonflag) {
    snprintf (tbuff, sizeof (tbuff),
        "\"duration\" : \"%s\"", tmutilToMS (atol (p), tbuff2, sizeof (tbuff2)));
    strlcat (jsbuff, ", ", sizeof (jsbuff));
    strlcat (jsbuff, tbuff, sizeof (jsbuff));
  }

  /* for marquee */
  snprintf (tbuff, sizeof (tbuff), "%s", p);
  strlcat (timerbuff, tbuff, sizeof (timerbuff));

  snprintf (tbuff, sizeof (tbuff), "%s%c", p, MSG_ARGS_RS);
  strlcat (statusbuff, tbuff, sizeof (statusbuff));

  if (jsonflag) {
    char    *data;
    dbidx_t dbidx;
    song_t  *song;

    musicqLen = musicqGetLen (mainData->musicQueue, mainData->musicqPlayIdx);
    snprintf (tbuff, sizeof (tbuff),
        "\"qlength\" : \"%d\"", musicqLen);
    strlcat (jsbuff, ", ", sizeof (jsbuff));
    strlcat (jsbuff, tbuff, sizeof (jsbuff));

    /* dance */
    data = musicqGetDance (mainData->musicQueue, mainData->musicqPlayIdx, 0);
    if (data == NULL) { data = ""; }
    snprintf (tbuff, sizeof (tbuff),
        "\"dance\" : \"%s\"", data);
    strlcat (jsbuff, ", ", sizeof (jsbuff));
    strlcat (jsbuff, tbuff, sizeof (jsbuff));

    dbidx = musicqGetByIdx (mainData->musicQueue, mainData->musicqPlayIdx, 0);
    song = dbGetByIdx (mainData->musicdb, dbidx);

    /* artist */
    data = songGetStr (song, TAG_ARTIST);
    if (data == NULL) { data = ""; }
    snprintf (tbuff, sizeof (tbuff),
        "\"artist\" : \"%s\"", data);
    strlcat (jsbuff, ", ", sizeof (jsbuff));
    strlcat (jsbuff, tbuff, sizeof (jsbuff));

    /* title */
    data = songGetStr (song, TAG_TITLE);
    if (data == NULL) { data = ""; }
    snprintf (tbuff, sizeof (tbuff),
        "\"title\" : \"%s\"", data);
    strlcat (jsbuff, ", ", sizeof (jsbuff));
    strlcat (jsbuff, tbuff, sizeof (jsbuff));

    strlcat (jsbuff, " }", sizeof (jsbuff));

    connSendMessage (mainData->conn, ROUTE_REMCTRL, MSG_PLAYER_STATUS_DATA, jsbuff);
  }

  connSendMessage (mainData->conn, ROUTE_PLAYERUI, MSG_PLAYER_STATUS_DATA, statusbuff);
  connSendMessage (mainData->conn, ROUTE_MANAGEUI, MSG_PLAYER_STATUS_DATA, statusbuff);
  if (mainData->marqueestarted) {
    connSendMessage (mainData->conn, ROUTE_MARQUEE, MSG_MARQUEE_TIMER, timerbuff);
  }
  logProcEnd (LOG_PROC, "mainSendPlayerStatus", "");
}

static void
mainSendMusicqStatus (maindata_t *mainData)
{
  char    tbuff [200];
  char    statusbuff [40];
  dbidx_t dbidx;

  logProcBegin (LOG_PROC, "mainSendMusicqStatus");

  statusbuff [0] = '\0';
  dbidx = musicqGetByIdx (mainData->musicQueue, mainData->musicqPlayIdx, 0);
  snprintf (tbuff, sizeof (tbuff), "%d%c", dbidx, MSG_ARGS_RS);
  strlcpy (statusbuff, tbuff, sizeof (statusbuff));

  if (dbidx >= 0) {
    connSendMessage (mainData->conn, ROUTE_PLAYERUI, MSG_MUSICQ_STATUS_DATA, statusbuff);
    connSendMessage (mainData->conn, ROUTE_MANAGEUI, MSG_MUSICQ_STATUS_DATA, statusbuff);
  }
  logProcEnd (LOG_PROC, "mainSendMusicqStatus", "");
}

static void
mainDanceCountsInit (maindata_t *mainData)
{
  nlist_t     *dc;
  ilistidx_t  didx;
  nlistidx_t  iteridx;

  logProcBegin (LOG_PROC, "mainDanceCountsInit");

  dc = dbGetDanceCounts (mainData->musicdb);

  if (mainData->danceCounts != NULL) {
    nlistFree (mainData->danceCounts);
  }

  mainData->danceCounts = nlistAlloc ("main-dancecounts", LIST_ORDERED, NULL);
  nlistSetSize (mainData->danceCounts, nlistGetCount (dc));

  nlistStartIterator (dc, &iteridx);
  while ((didx = nlistIterateKey (dc, &iteridx)) >= 0) {
    nlistSetNum (mainData->danceCounts, didx, nlistGetNum (dc, didx));
  }
  logProcEnd (LOG_PROC, "mainDanceCountsInit", "");
}

static void
mainParseIntNum (char *args, int *a, ilistidx_t *b)
{
  char            *p;
  char            *tokstr;

  p = strtok_r (args, MSG_ARGS_RS_STR, &tokstr);
  *a = atoi (p);
  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  *b = atol (p);
}

static void
mainParseIntStr (char *args, int *a, char **b)
{
  char            *p;
  char            *tokstr;

  p = strtok_r (args, MSG_ARGS_RS_STR, &tokstr);
  *a = atoi (p);
  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  *b = p;
}

static void
mainSendFinished (maindata_t *mainData)
{
  connSendMessage (mainData->conn, ROUTE_PLAYERUI, MSG_FINISHED, NULL);
  connSendMessage (mainData->conn, ROUTE_MANAGEUI, MSG_FINISHED, NULL);
  if (mainData->marqueestarted) {
    connSendMessage (mainData->conn, ROUTE_MARQUEE, MSG_FINISHED, NULL);
  }
}

static long
mainCalculateSongDuration (maindata_t *mainData, song_t *song, int playlistIdx)
{
  long  maxdur;
  long  dur;
  playlist_t  *playlist = NULL;
  long  plmaxdur;
  long  pldncmaxdur;
  long  songstart;
  long  songend;
  int   speed;
  ilistidx_t  danceidx;


  if (song == NULL) {
    return 0;
  }

  logProcBegin (LOG_PROC, "mainCalculateSongDuration");
  dur = songGetDurCache (song);
  if (dur >= 1) {
    logProcEnd (LOG_PROC, "mainCalculateSongDuration", "from-cache");
    return dur;
  }

  songstart = 0;
  speed = 100;
  dur = songGetNum (song, TAG_DURATION);

  maxdur = bdjoptGetNum (OPT_P_MAXPLAYTIME);
  songstart = songGetNum (song, TAG_SONGSTART);
  if (songstart < 0) {
    songstart = 0;
  }
  songend = songGetNum (song, TAG_SONGEND);
  if (songend < 0) {
    songend = 0;
  }
  speed = songGetNum (song, TAG_SPEEDADJUSTMENT);
  if (speed < 0) {
    speed = 100;
  }

  plmaxdur = LIST_VALUE_INVALID;
  if (playlistIdx != -1) {
    playlist_t    *playlist = NULL;

    playlist = nlistGetData (mainData->playlistCache, playlistIdx);
    plmaxdur = playlistGetConfigNum (playlist, PLAYLIST_MAX_PLAY_TIME);
  }

  /* apply songend if set to a reasonable value */
  logMsg (LOG_DBG, LOG_MAIN, "dur: %zd songstart: %zd songend: %zd",
      dur, songstart, songend);
  if (songend >= 10000 && dur > songend) {
    dur = songend;
    logMsg (LOG_DBG, LOG_MAIN, "dur-songend: %zd", dur);
  }
  /* adjust the song's duration by the songstart value */
  if (songstart > 0) {
    dur -= songstart;
    logMsg (LOG_DBG, LOG_MAIN, "dur-songstart: %zd", dur);
  }

  /* after adjusting the duration by song start/end, then adjust */
  /* the duration by the speed of the song */
  /* this is the real duration for the song */
  if (speed != 100) {
    double      drate;
    double      ddur;

    drate = (double) speed / 100.0;
    ddur = (double) dur / drate;
    dur = (ssize_t) ddur;
  }

  /* if the playlist has a maximum play time specified for a dance */
  /* it overrides any of the other max play times */
  danceidx = songGetNum (song, TAG_DANCE);
  pldncmaxdur = playlistGetDanceNum (playlist, danceidx, PLDANCE_MAXPLAYTIME);
  if (pldncmaxdur >= 5000) {
    dur = pldncmaxdur;
    logMsg (LOG_DBG, LOG_MAIN, "dur-plmaxdncdur: %zd", dur);
  } else {
    /* the playlist max-play-time overrides the global max-play-time */
    if (plmaxdur >= 5000) {
      if (dur > plmaxdur) {
        dur = plmaxdur;
        logMsg (LOG_DBG, LOG_MAIN, "dur-plmaxdur: %zd", dur);
      }
    } else if (dur > maxdur) {
      dur = maxdur;
      logMsg (LOG_DBG, LOG_MAIN, "dur-maxdur: %zd", dur);
    }
  }

  songSetDurCache (song, dur);
  logProcEnd (LOG_PROC, "mainCalculateSongDuration", "");
  return dur;
}

static playlistitem_t *
mainPlaylistItemCache (maindata_t *mainData, playlist_t *pl, int playlistIdx)
{
  playlistitem_t  *plitem;

  plitem = malloc (sizeof (playlistitem_t));
  plitem->playlist = pl;
  plitem->playlistIdx = playlistIdx;
  nlistSetData (mainData->playlistCache, playlistIdx, pl);
  return plitem;
}

static void
mainPlaylistItemFree (void *tplitem)
{
  playlistitem_t *plitem = tplitem;

  if (plitem != NULL) {
    /* the playlist data is owned by the playlistCache and is freed by it */
    free (plitem);
  }
}

static void
mainMusicqSetSuspend (maindata_t *mainData, char *args, bool value)
{
  int     mqidx;

  mqidx = atoi (args);
  if (mqidx < 0 || mqidx >= MUSICQ_MAX) {
    return;
  }
  mainData->changeSuspend [mqidx] = value;
}

static void
mainMusicQueueMix (maindata_t *mainData, char *args)
{
  int           mqidx;
  dbidx_t       musicqLen;
  dbidx_t       dbidx;
  int           danceIdx;
  song_t        *song = NULL;
  nlist_t       *songList = NULL;
  nlist_t       *danceCounts = NULL;
  dancesel_t    *dancesel = NULL;
  songsel_t     *songsel = NULL;
  int           totcount;
  int           currlen;

  mqidx = atoi (args);
  if (mqidx < 0 || mqidx >= MUSICQ_MAX) {
    return;
  }

  mainData->musicqManageIdx = mqidx;

  danceCounts = nlistAlloc ("mq-mix-counts", LIST_ORDERED, NULL);
  songList = nlistAlloc ("mq-mix-song-list", LIST_ORDERED, NULL);

  musicqLen = musicqGetLen (mainData->musicQueue, mqidx);
  logMsg (LOG_DBG, LOG_BASIC, "mix: mq len: %d", musicqLen);
  totcount = 0;
  /* skip the empty head; there is no idx = musicqLen */
  for (ssize_t i = 1; i < musicqLen; ++i) {
    int   plidx;

    dbidx = musicqGetByIdx (mainData->musicQueue, mqidx, i);
    if (dbidx < 0) {
      continue;
    }
    plidx = musicqGetPlaylistIdx (mainData->musicQueue, mqidx, i);
    nlistSetNum (songList, dbidx, plidx);
    song = dbGetByIdx (mainData->musicdb, dbidx);
    danceIdx = songGetNum (song, TAG_DANCE);
    if (danceIdx >= 0) {
      nlistIncrement (danceCounts, danceIdx);
      ++totcount;
    } else {
      logMsg (LOG_DBG, LOG_BASIC, "mix: unknown dance skipped dbidx:%d", dbidx);
    }
  }
  logMsg (LOG_DBG, LOG_BASIC, "mix: total count: %d", totcount);
  logMsg (LOG_DBG, LOG_BASIC, "mix: counts len: %d", nlistGetCount (danceCounts));

  /* for the purposes of a mix, the countlist passed to the dancesel alloc */
  /* and the dance counts used for selection are identical */

  musicqClear (mainData->musicQueue, mqidx, 1);
  /* should already be an empty head on the queue */

  dancesel = danceselAlloc (danceCounts);
  songsel = songselAlloc (mainData->musicdb, danceCounts, songList, NULL);

  currlen = 0;
  while (currlen < totcount) {
    /* as there is always an empty head on the music queue, */
    /* the prior-index must point at currlen + 1 */
    danceIdx = danceselSelect (dancesel, danceCounts, currlen + 1,
        mainMusicQueueHistory, mainData);
    song = songselSelect (songsel, danceIdx);
    if (song != NULL) {
      int   plidx;
      long  dur;

      songselSelectFinalize (songsel, danceIdx);
      logMsg (LOG_DBG, LOG_BASIC, "mix: (%d) d:%d/%s select: %s",
          currlen, danceIdx,
          danceGetStr (dancesel->dances, danceIdx, DANCE_DANCE),
          songGetStr (song, TAG_FILE));
      dbidx = songGetNum (song, TAG_DBIDX);
      danceselAddCount (dancesel, danceIdx);
      nlistDecrement (danceCounts, danceIdx);
      plidx = nlistGetNum (songList, dbidx);
      dur = mainCalculateSongDuration (mainData, song, plidx);
      musicqPush (mainData->musicQueue, mqidx, dbidx, plidx, dur);
      ++currlen;
    }
  }

  songselFree (songsel);
  danceselFree (dancesel);
  nlistFree (danceCounts);
  nlistFree (songList);

  mainData->musicqChanged [mqidx] = MAIN_CHG_START;
  mainData->marqueeChanged [mqidx] = true;
}

static void
mainPlaybackFinishProcess (maindata_t *mainData, const char *args)
{
  mainMusicQueueNext (mainData, args);
  ++mainData->pbfinishrcv;
}

static void
mainPlaybackSendSongFinish (maindata_t *mainData, const char *args)
{
  char    tmp [40];
  int     flag;
  dbidx_t dbidx;

  flag = atoi (args);
  if (flag) {
    dbidx = musicqGetCurrent (mainData->musicQueue, mainData->musicqPlayIdx);
    snprintf (tmp, sizeof (tmp), "%d", dbidx);
    connSendMessage (mainData->conn, ROUTE_PLAYERUI, MSG_SONG_FINISH, tmp);
  }
}

static void
mainStatusRequest (maindata_t *mainData, bdjmsgroute_t routefrom)
{
  char  tmp [40];

  for (int i = 0; i < MUSICQ_MAX; ++i) {
    mainData->musicqChanged [i] = MAIN_CHG_START;
  }
  snprintf (tmp, sizeof (tmp), "%d", mainData->musicqManageIdx);
  connSendMessage (mainData->conn, routefrom, MSG_MAIN_CURR_MANAGE, tmp);
  snprintf (tmp, sizeof (tmp), "%d", mainData->musicqPlayIdx);
  connSendMessage (mainData->conn, routefrom, MSG_MAIN_CURR_PLAY, tmp);
  mainSendMusicqStatus (mainData);
  /* send the last player state that has been recorded */
  snprintf (tmp, sizeof (tmp), "%d", mainData->playerState);
  connSendMessage (mainData->conn, routefrom, MSG_PLAYER_STATE, tmp);
}

static void
mainChkMusicq (maindata_t *mainData, bdjmsgroute_t routefrom)
{
  char    tmp [2000];
  dbidx_t dbidx;
  dbidx_t qdbidx;
  char    *title;
  char    *dance;
  song_t  *song;
  char    *songfn;

  dbidx = -1;
  qdbidx = -1;
  title = MSG_ARGS_EMPTY_STR;
  dance = MSG_ARGS_EMPTY_STR;
  songfn = MSG_ARGS_EMPTY_STR;
  if (mainData->playerState == PL_STATE_PLAYING) {
    dbidx = musicqGetByIdx (mainData->musicQueue, mainData->musicqPlayIdx, 0);
    title = musicqGetData (mainData->musicQueue, mainData->musicqPlayIdx, 0, TAG_TITLE);
    dance = mainSongGetDanceDisplay (mainData, 0);
    song = dbGetByIdx (mainData->musicdb, dbidx);
    songfn = songGetStr (song, TAG_FILE);
  } else {
    qdbidx = musicqGetCurrent (mainData->musicQueue, mainData->musicqPlayIdx);
  }

  snprintf (tmp, sizeof (tmp),
      "mqmanage%c%d%c"
      "mqplay%c%d%c"
      "mqmlen%c%ld%c"
      "mqplen%c%ld%c"
      "dbidx%c%d%c"
      "qdbidx%c%d%c"
      "m-songfn%c%s%c"
      "title%c%s%c"
      "dance%c%s%c"
      "songplaysentcount%c%d",
      MSG_ARGS_RS, mainData->musicqManageIdx, MSG_ARGS_RS,
      MSG_ARGS_RS, mainData->musicqPlayIdx, MSG_ARGS_RS,
      MSG_ARGS_RS, musicqGetLen (mainData->musicQueue, mainData->musicqManageIdx), MSG_ARGS_RS,
      MSG_ARGS_RS, musicqGetLen (mainData->musicQueue, mainData->musicqPlayIdx), MSG_ARGS_RS,
      MSG_ARGS_RS, dbidx, MSG_ARGS_RS,
      MSG_ARGS_RS, qdbidx, MSG_ARGS_RS,
      MSG_ARGS_RS, songfn, MSG_ARGS_RS,
      MSG_ARGS_RS, title, MSG_ARGS_RS,
      MSG_ARGS_RS, dance, MSG_ARGS_RS,
      MSG_ARGS_RS, mainData->songplaysentcount);
  connSendMessage (mainData->conn, routefrom, MSG_CHK_MAIN_MUSICQ, tmp);
}

