#ifndef INC_PATHBLD_H
#define INC_PATHBLD_H

typedef enum {
  PATHBLD_MP_NONE        = 0x0000,
  PATHBLD_MP_HOSTNAME    = 0x0001,
  PATHBLD_MP_USEIDX      = 0x0002,
  PATHBLD_MP_TMPDIR      = 0x0004,   // defaults to data/
  PATHBLD_MP_EXECDIR     = 0x0008,
  PATHBLD_MP_MAINDIR     = 0x0010,
  PATHBLD_MP_MUSICDIR    = 0x0020,
} datautil_mp_t;

char *        pathbldMakePath (char *buff, size_t buffsz, const char *subpath,
                  const char *base, const char *extension, int flags);

#endif /* INC_PATHBLD_H */
