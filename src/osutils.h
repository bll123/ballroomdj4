#ifndef INC_OSUTILS_H
#define INC_OSUTILS_H

#include "config.h"

#include <sys/types.h>
#include <dirent.h>
#include <wchar.h>

#if _hdr_winsock2
# include <winsock2.h>
#endif
#if _hdr_windows
# include <windows.h>
#endif

enum {
  OS_PROC_NONE    = 0x0000,
  OS_PROC_DETACH  = 0x0001,
  OS_PROC_WAIT    = 0x0002,
};

typedef struct {
  DIR       *dh;
  char      *dirname;
#if _typ_HANDLE
  HANDLE    dhandle;
#endif
} dirhandle_t;

#if _lib_MultiByteToWideChar
# define OS_FS_CHAR_TYPE wchar_t
#else
# define OS_FS_CHAR_TYPE char
#endif
#define OS_FS_CHAR_SIZE sizeof (OS_FS_CHAR_TYPE)

double        dRandom (void);
void          sRandom (void);
pid_t         osProcessStart (char *targv[], int flags,
                void **handle, char *outfname);
void          * osToFSFilename (const char *fname);
char          * osFromFSFilename (const void *fname);
dirhandle_t   * osDirOpen (const char *dir);
char *        osDirIterate (dirhandle_t *dirh);
void          osDirClose (dirhandle_t *dirh);

#endif /* INC_OSUTILS_H */
