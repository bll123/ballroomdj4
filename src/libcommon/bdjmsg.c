#include "config.h"


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "bdjmsg.h"
#include "bdjstring.h"

#define LSZ         4
#define MSG_RS      '~'

typedef struct {
  char            *text;
} bdjmsgtxt_t;

/* for debugging */
static bdjmsgtxt_t routetxt [ROUTE_MAX] = {
  [ROUTE_CLICOMM] = { "CLICOMM" },
  [ROUTE_CONFIGUI] = { "CONFIGUI" },
  [ROUTE_DBTAG] = { "DBTAG" },
  [ROUTE_DBUPDATE] = { "DBUPDATE" },
  [ROUTE_MAIN] = { "MAIN" },
  [ROUTE_MANAGEUI] = { "MANAGEUI" },
  [ROUTE_MARQUEE] = { "MARQUEE" },
  [ROUTE_MOBILEMQ] = { "MOBILEMQ" },
  [ROUTE_NONE] = { "NONE" },
  [ROUTE_PLAYER] = { "PLAYER" },
  [ROUTE_PLAYERUI] = { "PLAYERUI" },
  [ROUTE_REMCTRL] = { "REMCTRL" },
  [ROUTE_STARTERUI] = { "STARTERUI" },
};

/* for debugging */
static bdjmsgtxt_t msgtxt [MSG_MAX] = {
  [MSG_NULL] = { "NULL" },
  [MSG_EXIT_REQUEST] = { "EXIT_REQUEST" },
  [MSG_HANDSHAKE] = { "HANDSHAKE" },
  [MSG_SOCKET_CLOSE] = { "SOCKET_CLOSE" },
  [MSG_GET_STATUS] = { "GET_STATUS" },
  [MSG_MUSICQ_INSERT] = { "MUSICQ_INSERT" },
  [MSG_MUSICQ_MOVE_DOWN] = { "MUSICQ_MOVE_DOWN" },
  [MSG_MUSICQ_MOVE_TOP] = { "MUSICQ_MOVE_TOP" },
  [MSG_MUSICQ_MOVE_UP] = { "MUSICQ_MOVE_UP" },
  [MSG_MUSICQ_REMOVE] = { "MUSICQ_REMOVE" },
  [MSG_MUSICQ_SET_PLAYBACK] = { "MUSICQ_SET_PLAYBACK" },
  [MSG_MUSICQ_TOGGLE_PAUSE] = { "MUSICQ_TOGGLE_PAUSE" },
  [MSG_MUSICQ_TRUNCATE] = { "MUSICQ_TRUNCATE" },
  [MSG_PLAYLIST_CLEARPLAY] = { "PLAYLIST_CLEARPLAY" },
  [MSG_PLAY_PLAY] = { "PLAY_PLAY" },
  [MSG_PLAY_PLAYPAUSE] = { "PLAY_PLAYPAUSE" },
  [MSG_QUEUE_CLEAR] = { "QUEUE_CLEAR" },
  [MSG_QUEUE_DANCE_5] = { "QUEUE_DANCE_5" },
  [MSG_QUEUE_DANCE] = { "QUEUE_DANCE" },
  [MSG_QUEUE_PLAYLIST] = { "QUEUE_PLAYLIST" },
  [MSG_QUEUE_PLAY_ON_ADD] = { "QUEUE_PLAY_ON_ADD" },
  [MSG_QUEUE_SWITCH_EMPTY] = { "QUEUE_SWITCH_EMPTY" },
  [MSG_PLAYER_VOL_MUTE] = { "PLAYER_VOL_MUTE" },
  [MSG_PLAYER_VOLUME] = { "PLAYER_VOLUME" },
  [MSG_PLAY_FADE] = { "PLAY_FADE" },
  [MSG_PLAY_NEXTSONG] = { "PLAY_NEXTSONG" },
  [MSG_PLAY_PAUSEATEND] = { "PLAY_PAUSEATEND" },
  [MSG_PLAY_PAUSE] = { "PLAY_PAUSE" },
  [MSG_PLAY_REPEAT] = { "PLAY_REPEAT" },
  [MSG_PLAY_SEEK] = { "PLAY_SEEK" },
  [MSG_PLAY_SONG_BEGIN] = { "PLAY_SONG_BEGIN" },
  [MSG_PLAY_SPEED] = { "PLAY_SPEED" },
  [MSG_PLAY_STOP] = { "PLAY_STOP" },
  [MSG_SONG_PLAY] = { "SONG_PLAY" },
  [MSG_SONG_PREP] = { "SONG_PREP" },
  [MSG_PLAY_PAUSEATEND_STATE] = { "PLAY_PAUSEATEND_STATE" },
  [MSG_PLAYBACK_BEGIN] = { "PLAYBACK_BEGIN" },
  [MSG_PLAYBACK_STOP] = { "PLAYBACK_STOP" },
  [MSG_PLAYBACK_FINISH] = { "PLAYBACK_FINISH" },
  [MSG_PLAYER_STATE] = { "PLAYER_STATE" },
  [MSG_PLAYER_STATUS_DATA] = { "PLAYER_STATUS_DATA" },
  [MSG_MUSIC_QUEUE_DATA] = { "MUSIC_QUEUE_DATA" },
  [MSG_QUEUE_SWITCH] = { "QUEUE_SWITCH" },
  [MSG_SONG_SELECT] = { "SONG_SELECT" },
  [MSG_FINISHED] = { "FINISHED" },
  [MSG_DANCE_LIST_DATA] = { "DANCE_LIST_DATA" },
  [MSG_GET_DANCE_LIST] = { "GET_DANCE_LIST" },
  [MSG_GET_PLAYLIST_LIST] = { "GET_PLAYLIST_LIST" },
  [MSG_MARQUEE_DATA] = { "MARQUEE_DATA" },
  [MSG_MUSICQ_STATUS_DATA] = { "MUSICQ_STATUS_DATA" },
  [MSG_PLAYLIST_LIST_DATA] = { "PLAYLIST_LIST_DATA" },
  [MSG_MARQUEE_TIMER] = { "MARQUEE_TIMER" },
  [MSG_MARQUEE_SET_FONT_SZ] = { "MARQUEE_SET_FONT_SZ" },
  [MSG_MARQUEE_IS_MAX] = { "MARQUEE_IS_MAX" },
  [MSG_MARQUEE_FONT_SIZES] = { "MARQUEE_FONT_SIZES" },
  [MSG_DB_FILE_TAGS] = { "DB_FILE_TAGS" },
  [MSG_DB_STATUS] = { "DB_STATUS" },
  [MSG_DB_FILE_CHK] = { "DB_FILE_CHK" },
};

size_t
msgEncode (bdjmsgroute_t routefrom, bdjmsgroute_t route,
    bdjmsgmsg_t msg, char *args, char *msgbuff, size_t mlen)
{
  size_t      len;


  if (args == NULL) {
    args = "";
  }
  len = (size_t) snprintf (msgbuff, mlen, "%0*d%c%0*d%c%0*d%c%s",
      LSZ, routefrom, MSG_RS, LSZ, route, MSG_RS, LSZ, msg, MSG_RS, args);
  ++len;
  return len;
}

void
msgDecode (char *msgbuff, bdjmsgroute_t *routefrom, bdjmsgroute_t *route,
    bdjmsgmsg_t *msg, char *args, size_t alen)
{
  char        *p = NULL;

  p = msgbuff;
  *routefrom = (bdjmsgroute_t) atol (p);
  p += LSZ + 1;
  *route = (bdjmsgroute_t) atol (p);
  p += LSZ + 1;
  *msg = (bdjmsgmsg_t) atol (p);
  p += LSZ + 1;
  if (args != NULL) {
    *args = '\0';
    strlcpy (args, p, alen);
  }
}

inline char *
msgDebugText (bdjmsgmsg_t msg)
{
  return msgtxt [msg].text;
}

inline char *
msgRouteDebugText (bdjmsgroute_t route)
{
  return routetxt [route].text;
}