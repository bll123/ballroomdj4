#ifndef INC_VOLUME
#define INC_VOLUME

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

#include "dylib.h"

typedef struct volsinklist volsinklist_t;

typedef enum {
  VOL_GET,
  VOL_SET,
  VOL_HAVE_SINK_LIST,
  VOL_GETSINKLIST,
} volaction_t;

typedef struct {
  dlhandle_t  *dlHandle;
  int         (*volumeProcess) (volaction_t, char *, int *, volsinklist_t *);
  void        (*volumeDisconnect) (void);
} volume_t;

volume_t  *volumeInit (const char *volpkg);
void      volumeFree (volume_t *volume);
bool      volumeHaveSinkList (volume_t *volume);
void      volumeSinklistInit (volsinklist_t *sinklist);
int       volumeGet (volume_t *volume, char *sinkname);
int       volumeSet (volume_t *volume, char *sinkname, int vol);
int       volumeGetSinkList (volume_t *volume, char *sinkname, volsinklist_t *sinklist);
void      volumeFreeSinkList (volsinklist_t *sinklist);

int       volumeProcess (volaction_t action, char *sinkname,
              int *vol, volsinklist_t *sinklist);
void      volumeDisconnect (void);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_VOLUME */
