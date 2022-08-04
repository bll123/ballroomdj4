#ifndef INC_BDJLOG_H
#define INC_BDJLOG_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "bdj4.h"
#include "fileutil.h"

/*
 * log tags:
 *  ck - check_all
 *  in - installer
 *  up - updater
 *  ai - alt-installer
 *  st - starter
 *  hp - helper
 *  pu - player ui
 *  mu - management ui
 *  bc - bpm counter
 *  db - database update
 *  dt - database tag
 *  cu - configuration ui
 *  m  - main
 *  p  - player
 *  mq - marquee
 *  rc - mobile remote control
 *  mm - mobile marquee
 *
 */


typedef uint32_t   loglevel_t;

enum {
  LOG_NONE            = 0x00000000,
  LOG_IMPORTANT       = 0x00000001,  // 1
  LOG_BASIC           = 0x00000002,  // 2
  LOG_MSGS            = 0x00000004,  // 4
  LOG_MAIN            = 0x00000008,  // 8
  LOG_ACTIONS         = 0x00000010,  // 16
  LOG_LIST            = 0x00000020,  // 32
  LOG_SONGSEL         = 0x00000040,  // 64
  LOG_DANCESEL        = 0x00000080,  // 128
  LOG_VOLUME          = 0x00000100,  // 256
  LOG_SOCKET          = 0x00000200,  // 512
  LOG_DB              = 0x00000400,  // 1024
  LOG_RAFILE          = 0x00000800,  // 2048
  LOG_PROC            = 0x00001000,  // 4096
  LOG_PLAYER          = 0x00002000,  // 8192
  LOG_DATAFILE        = 0x00004000,  // 16384
  LOG_PROCESS         = 0x00008000,  // 32768
  LOG_WEBSRV          = 0x00010000,  // 65536
  LOG_WEBCLIENT       = 0x00020000,  // 131072
  LOG_DBUPDATE        = 0x00040000,  // 262144
  LOG_PROGSTATE       = 0x00080000,  // 524288
  LOG_STDERR          = 0x00100000,  // 1048576
  LOG_REDIR_INST      = 0x00200000,  // 2097152
  LOG_ALL             = (~LOG_STDERR & ~LOG_REDIR_INST),
};

typedef enum {
  LOG_ERR,
  LOG_SESS,
  LOG_DBG,
  LOG_INSTALL,
  LOG_GTK,
  LOG_MAX
} logidx_t;

typedef struct {
  filehandle_t  fhandle;
  int           opened;
  int           indent;
  ssize_t       level;
  const char    *processTag;
} bdjlog_t;

#define LOG_ERROR_NAME    "logerror"
#define LOG_SESSION_NAME  "logsession"
#define LOG_DEBUG_NAME    "logdbg"
#define LOG_INSTALL_NAME  "loginstall"
#define LOG_GTK_NAME      "loggtk"
#define LOG_EXTENSION     ".txt"

enum {
  LOG_MAX_BUFF    = 1024,
};

#define logProcBegin(lvl,tag)   rlogProcBegin (lvl, tag, __FILE__, __LINE__)
#define logProcEnd(lvl,tag,suffix)  rlogProcEnd (lvl, tag, suffix, __FILE__, __LINE__)
#define logError(msg)           rlogError (msg, errno, __FILE__, __LINE__)
#define logMsg(idx,lvl,fmt,...) rlogVarMsg (idx, lvl, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

bdjlog_t *  logOpen (const char *fn, const char *processtag);
bdjlog_t *  logOpenAppend (const char *fn, const char *processtag);
void logClose (logidx_t idx);
bool logCheck (logidx_t idx, loglevel_t level);
void logSetLevel (logidx_t idx, loglevel_t level, const char *processtag);
void logStart (const char *processnm, const char *processtag, loglevel_t level);
void logStartAppend (const char *processnm, const char *processtag, loglevel_t level);
void logEnd (void);
void logBacktraceHandler (int sig);
char * plstateDebugText (playerstate_t plstate);
void logBasic (const char *fmt, ...);

/* needed by the #defines */
void rlogProcBegin (loglevel_t level, const char *tag, const char *fn, int line);
void rlogProcEnd (loglevel_t level, const char *tag, const char *suffix, const char *fn, int line);
void rlogError (const char *msg, int err, const char *fn, int line);
void rlogVarMsg (logidx_t, loglevel_t level, const char *fn, int line, const char *fmt, ...);

#endif /* INC_BDJLOG_H */
