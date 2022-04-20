#include "config.h"


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "bdj4.h"
#include "log.h"
#include "tmutil.h"
#include "fileutil.h"
#include "pathbld.h"
#include "bdjstring.h"

static void         rlogStart (const char *processnm,
                        const char *processtag, int truncflag,
                        loglevel_t level);
static bdjlog_t *   rlogOpen (const char *fn,
                        const char *processtag, int truncflag);
static void         logInit (void);
static const char * logTail (const char *fn);
void                logDumpLevel (loglevel_t level);

static bdjlog_t *syslogs [LOG_MAX];
static char *   logbasenm [LOG_MAX];
static int      initialized = 0;

bdjlog_t *
logOpen (const char *fn, const char *processtag)
{
  bdjlog_t *l = rlogOpen (fn, processtag, 1);
  return l;
}

bdjlog_t *
logOpenAppend (const char *fn, const char *processtag)
{
  bdjlog_t *l = rlogOpen (fn, processtag, 0);
  return l;
}

void
logClose (logidx_t idx)
{
  bdjlog_t      *l;

  l = syslogs [idx];
  if (l == NULL) {
    return;
  }

  fileCloseShared (&l->fhandle);
  l->opened = 0;
}

void
rlogProcBegin (loglevel_t level, const char *tag, const char *fn, int line)
{
  if (! logCheck (LOG_DBG, level)) {
    return;
  }
  rlogVarMsg (LOG_DBG, level, fn, line, "- %s begin", tag);
  syslogs [LOG_DBG]->indent += 2;
}

void
rlogProcEnd (loglevel_t level, const char *tag, const char *suffix, const char *fn, int line)
{
  if (! logCheck (LOG_DBG, level)) {
    return;
  }
  syslogs [LOG_DBG]->indent -= 2;
  if (syslogs [LOG_DBG]->indent < 0) {
    syslogs [LOG_DBG]->indent = 0;
  }
  rlogVarMsg (LOG_DBG, level, fn, line, "- %s end %s", tag, suffix);
}

void
rlogError (const char *msg, int err, const char *fn, int line)
{
  if (syslogs [LOG_ERR] == NULL) {
    return;
  }
  rlogVarMsg (LOG_ERR, LOG_IMPORTANT, fn, line, "err: %s %d %s", msg, err, strerror (err));
  rlogVarMsg (LOG_DBG, LOG_IMPORTANT, fn, line, "err: %s %d %s", msg, err, strerror (err));
}

void
rlogVarMsg (logidx_t idx, loglevel_t level,
    const char *fn, int line, const char *fmt, ...)
{
  bdjlog_t      *l;
  char          ttm [40];
  char          tbuff [512];
  char          wbuff [1024];
  char          tfn [MAXPATHLEN];
  va_list       args;
  size_t        wlen;


  if (! logCheck (idx, level)) {
    return;
  }
  l = syslogs [idx];

  tmutilTstamp (ttm, sizeof (ttm));
  *tbuff = '\0';
  *tfn = '\0';
  if (fmt != NULL) {
    va_start (args, fmt);
    vsnprintf (tbuff, sizeof (tbuff), fmt, args);
    va_end (args);
  }
  if (fn != NULL) {
    snprintf (tfn, sizeof (tfn), "(%s: %d)", logTail (fn), line);
  }
  wlen = (size_t) snprintf (wbuff, sizeof (wbuff),
      "%s: %-2s %*s%s %s\n", ttm, l->processTag, l->indent, "", tbuff, tfn);
  fileWriteShared (&l->fhandle, wbuff, wlen);
}

void
logSetLevel (logidx_t idx, loglevel_t level)
{
  syslogs [idx]->level = level;
  logDumpLevel (level);
}

/* these routines act upon all three open logs */

void
logStart (const char *processnm, const char *processtag, loglevel_t level)
{
  rlogStart (processnm, processtag, 1, level);
}

void
logStartAppend (const char *processnm, const char *processtag, loglevel_t level)
{
  rlogStart (processnm, processtag, 0, level);
}

void
logEnd (void)
{
  for (logidx_t idx = LOG_ERR; idx < LOG_MAX; ++idx) {
    if (syslogs [idx] != NULL) {
      logClose (idx);
      free (syslogs [idx]);
      syslogs [idx] = NULL;
    }
  }
}

bool
logCheck (logidx_t idx, loglevel_t level)
{
  bdjlog_t      *l;

  l = syslogs [idx];
  if (l == NULL || ! l->opened) {
    return false;
  }
  if (! (level & l->level)) {
    return false;
  }
  return true;
}

/* internal routines */

static void
rlogStart (const char *processnm, const char *processtag,
    int truncflag, loglevel_t level)
{
  char      tnm [MAXPATHLEN];
  char      tdt [40];

  logInit ();
  tmutilDstamp (tdt, sizeof (tdt));

  for (logidx_t idx = LOG_ERR; idx < LOG_MAX; ++idx) {
    pathbldMakePath (tnm, sizeof (tnm), "", logbasenm [idx], LOG_EXTENSION,
        PATHBLD_MP_HOSTNAME | PATHBLD_MP_USEIDX);
    syslogs [idx] = rlogOpen (tnm, processtag, truncflag);
    syslogs [idx]->level = level;
    if (idx != LOG_INSTALL && idx != LOG_GTK) {
      rlogVarMsg (idx, LOG_IMPORTANT, NULL, 0, "=== %s started %s", processnm, tdt);
    }
  }
  logDumpLevel (level);
}

static bdjlog_t *
rlogOpen (const char *fn, const char *processtag, int truncflag)
{
  bdjlog_t      *l = NULL;
  int           rc;


  l = malloc (sizeof (bdjlog_t));
  assert (l != NULL);

  l->opened = 0;
  l->indent = 0;
  l->level = LOG_IMPORTANT;
  l->processTag = processtag;
  rc = fileOpenShared (fn, truncflag, &l->fhandle);
  if (rc < 0) {
    fprintf (stderr, "%s: Unable to open %s %d %s\n",
        processtag, fn, errno, strerror (errno));
  }
  l->opened = 1;
  return l;
}

static void
logInit (void)
{
  if (! initialized) {
    logbasenm [LOG_ERR] = LOG_ERROR_NAME;
    logbasenm [LOG_SESS] = LOG_SESSION_NAME;
    logbasenm [LOG_DBG] = LOG_DEBUG_NAME;
    logbasenm [LOG_INSTALL] = LOG_INSTALL_NAME;
    logbasenm [LOG_GTK] = LOG_GTK_NAME;
    initialized = 1;
  }
}

const char *
logTail (const char *fn)
{
  const char    *p;

  p = fn + strlen (fn) - 1;
  while (p != fn && *p != '/' && *p != '\\') {
    --p;
  }
  if (p != fn) {
    ++p;
  }
  return p;
}

void
logDumpLevel (loglevel_t level)
{
  char          tbuff [MAXPATHLEN];

  *tbuff = '\0';
  strlcat (tbuff, "log-dump: ", MAXPATHLEN);
  if ((level & LOG_IMPORTANT) == LOG_IMPORTANT) {
    strlcat (tbuff, "important ", MAXPATHLEN);
  }
  if ((level & LOG_BASIC) == LOG_BASIC) {
    strlcat (tbuff, "basic ", MAXPATHLEN);
  }
  if ((level & LOG_MAIN) == LOG_MAIN) {
    strlcat (tbuff, "main ", MAXPATHLEN);
  }
  if ((level & LOG_DATAFILE) == LOG_DATAFILE) {
    strlcat (tbuff, "datafile ", MAXPATHLEN);
  }
  if ((level & LOG_LIST) == LOG_LIST) {
    strlcat (tbuff, "list ", MAXPATHLEN);
  }
  if ((level & LOG_PLAYER) == LOG_PLAYER) {
    strlcat (tbuff, "player ", MAXPATHLEN);
  }
  if ((level & LOG_PROCESS) == LOG_PROCESS) {
    strlcat (tbuff, "process ", MAXPATHLEN);
  }
  if ((level & LOG_VOLUME) == LOG_VOLUME) {
    strlcat (tbuff, "volume ", MAXPATHLEN);
  }
  if ((level & LOG_SOCKET) == LOG_SOCKET) {
    strlcat (tbuff, "socket ", MAXPATHLEN);
  }
  if ((level & LOG_DB) == LOG_DB) {
    strlcat (tbuff, "db ", MAXPATHLEN);
  }
  if ((level & LOG_RAFILE) == LOG_RAFILE) {
    strlcat (tbuff, "rafile ", MAXPATHLEN);
  }
  rlogVarMsg (LOG_SESS, LOG_IMPORTANT, NULL, 0, tbuff);
}
