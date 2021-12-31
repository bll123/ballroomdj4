#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "sockh.h"
#include "sock.h"
#include "tmutil.h"
#include "log.h"
#include "bdjmsg.h"

void
sockhMainLoop (uint16_t listenPort, sockProcessMsg_t msgProc,
            sockOtherProcessing_t otherProc)
{
  Sock_t      listenSock;
  Sock_t      msgsock;
  int         err;
  sockinfo_t  *si;
  char        msgbuff [1024];
  size_t      len;

  listenSock = sockServer (listenPort, &err);
  si = sockAddCheck (si, listenSock);

  while ((msgsock = sockCheck (si)) >= 0) {
    if (msgsock == listenSock) {
      Sock_t clsock = sockAccept (listenSock, &err);
      si = sockAddCheck (si, clsock);
    } else {
      char *rval = sockReadBuff (msgsock, &len, msgbuff, sizeof (msgbuff));
      if (rval != NULL) {
        long      route;
        long      msg;

        // ### FIX handle args
        msgDecode (msgbuff, &route, &msg, NULL);
        switch (msg) {
          case MSG_FORCE_EXIT: {
            exit (1);
            break;
          }
          case MSG_CLOSE_SOCKET: {
            sockRemoveCheck (si, msgsock);
            sockClose (msgsock);
            break;
          }
          default: {
            msgProc (route, msg, NULL);
          }
        } /* switch */
      } /* msg is ok */
    } /* message from client */

    otherProc ();
    msleep (SOCK_MAINLOOP_TIMEOUT);
  } /* wait for a message */

  sockhCloseClients (si);
  sockFreeCheck (si);
  sockClose (listenSock);
}

void
sockhRequestClose (sockinfo_t *sockinfo)
{
  int               rc;

  if (sockinfo == NULL) {
    return;
  }

  for (size_t i = 0; i < (size_t) sockinfo->count; ++i) {
    Sock_t tsock = sockinfo->socklist[i];
    if (socketInvalid (tsock)) {
      continue;
    }

    //### FIX TODO  send a msg, request close
  }

  return;
}

void
sockhCloseClients (sockinfo_t *sockinfo)
{
  int               rc;

  if (sockinfo == NULL) {
    return;
  }

  for (size_t i = 0; i < (size_t) sockinfo->count; ++i) {
    Sock_t tsock = sockinfo->socklist[i];
    if (socketInvalid (tsock)) {
      continue;
    }

    sockRemoveCheck (sockinfo, tsock);
    sockClose (tsock);
  }

  return;
}
