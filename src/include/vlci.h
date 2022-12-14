#ifndef INC_VLCI_H
#define INC_VLCI_H

#include "config.h"

/* winsock2.h should come before windows.h */
#if _hdr_winsock2
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wmissing-declarations"
# include <winsock2.h>
# pragma clang diagnostic pop
#endif
#if _hdr_windows
# include <windows.h>
#endif

#include <vlc/vlc.h>
#include <vlc/libvlc_version.h>

#include "volsink.h"

typedef struct {
  libvlc_instance_t     *inst;
  char                  version [40];
  libvlc_media_t        *m;
  libvlc_media_player_t *mp;
  libvlc_state_t        state;
  int                   argc;
  char                  **argv;
  char                  *device;
  int                   initialvolume;
} vlcData_t;

ssize_t           vlcGetDuration (vlcData_t *vlcData);
ssize_t           vlcGetTime (vlcData_t *vlcData);
int               vlcIsPlaying (vlcData_t *vlcData);
int               vlcIsPaused (vlcData_t *vlcData);
int               vlcStop (vlcData_t *vlcData);
int               vlcPause (vlcData_t *vlcData);
int               vlcPlay (vlcData_t *vlcData);
ssize_t           vlcSeek (vlcData_t *vlcData, ssize_t dpos);
double            vlcRate (vlcData_t *vlcData, double drate);
int               vlcHaveAudioDevList (void);
int               vlcAudioDevSet (vlcData_t *vlcData, const char *dev);
#if _lib_libvlc_audio_output_device_enum
int               vlcAudioDevList (vlcData_t *vlcData, volsinklist_t *sinklist);
#endif
char *            vlcVersion (vlcData_t *vlcData);
libvlc_state_t    vlcState (vlcData_t *vlcData);
const char *      vlcStateStr (vlcData_t *vlcData);
void              noInitialVolume (vlcData_t *vlcData);
int               vlcMedia (vlcData_t *vlcdata, const char *fn);
vlcData_t *       vlcInit (int vlcargc, char *vlcargv [], char *vlcopt []);
void              vlcClose (vlcData_t *vlcData);
void              vlcRelease (vlcData_t *vlcData);
void              vlcExitHandler (vlcData_t *vlcData);
void              vlcEventHandler (const struct libvlc_event_t *event, void *);

#endif /* INC_VLCI_H */
