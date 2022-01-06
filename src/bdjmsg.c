#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bdjmsg.h"
#include "bdjstring.h"

#define LSZ         4
#define MSG_RS      '~'

size_t
msgEncode (long route, long msg, char *args, char *msgbuff, size_t mlen)
{
  if (args == NULL) {
    args = "";
  }
  size_t len = (size_t) snprintf (msgbuff, mlen, "%0*ld%c%0*ld%c%s",
      LSZ, route, MSG_RS, LSZ, msg, MSG_RS, args);
  ++len;
  return len;
}

void
msgDecode (char *msgbuff, long *route, long *msg, char *args)
{
  *route = atol (msgbuff);
  char *p = msgbuff + LSZ + 1;
  *msg = atol (p);
  if (args != NULL) {
    *args = '\0';
    p += LSZ + 1;
    strlcpy (args, p, BDJMSG_MAX);
  }
}
