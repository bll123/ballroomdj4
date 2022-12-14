#ifndef INC_MANAGEUI_H
#define INC_MANAGEUI_H

#include "conn.h"
#include "msgparse.h"
#include "nlist.h"
#include "playlist.h"
#include "ui.h"

enum {
  MANAGE_DB_COUNT_SAVE = 10,
};

/* managepl.c */
typedef struct managepl managepl_t;

managepl_t *managePlaylistAlloc (UIWidget *window, nlist_t *options,
    UIWidget *statusMsg);
void managePlaylistFree (managepl_t *managepl);
void managePlaylistSetLoadCallback (managepl_t *managepl, UICallback *uicb);
void manageBuildUIPlaylist (managepl_t *managepl, UIWidget *vboxp);
uimenu_t *managePlaylistMenu (managepl_t *managepl, UIWidget *menubar);
void managePlaylistSave (managepl_t *managepl);
void managePlaylistLoadCheck (managepl_t *managepl);
void managePlaylistLoadFile (void *udata, const char *fn);

/* managepltree.c */
typedef struct managepltree managepltree_t;

managepltree_t *managePlaylistTreeAlloc (UIWidget *statusMsg);
void managePlaylistTreeFree (managepltree_t *managepltree);
void manageBuildUIPlaylistTree (managepltree_t *managepltree, UIWidget *vboxp,  UIWidget *tophbox);
void managePlaylistTreePopulate (managepltree_t *managepltree, playlist_t *pl);
bool managePlaylistTreeIsChanged (managepltree_t *managepltree);
void managePlaylistTreeUpdatePlaylist (managepltree_t *managepltree);

/* manageseq.c */
typedef struct manageseq manageseq_t;

manageseq_t *manageSequenceAlloc (UIWidget *window, nlist_t *options,
    UIWidget *statusMsg);
void manageSequenceFree (manageseq_t *manageseq);
void manageSequenceSetLoadCallback (manageseq_t *manageseq, UICallback *uicb);
void manageBuildUISequence (manageseq_t *manageseq, UIWidget *vboxp);
uimenu_t *manageSequenceMenu (manageseq_t *manageseq, UIWidget *menubar);
void manageSequenceSave (manageseq_t *manageseq);
void manageSequenceLoadCheck (manageseq_t *manageseq);

/* managedb.c */
typedef struct managedb managedb_t;

managedb_t *manageDbAlloc (UIWidget *window, nlist_t *options, UIWidget *statusMsg, conn_t *conn);
void  manageDbFree (managedb_t *managedb);
void  manageBuildUIUpdateDatabase (managedb_t *managedb, UIWidget *vboxp);
bool  manageDbChg (void *udata);
void  manageDbProgressMsg (managedb_t *managedb, char *args);
void  manageDbStatusMsg (managedb_t *managedb, char *args);
void  manageDbFinish (managedb_t *managedb, int routefrom);
void  manageDbClose (managedb_t *managedb);

/* managemisc.c */
void manageSetStatusMsg (UIWidget *statusMsg, const char *msg);
void manageRenamePlaylistFiles (const char *oldname, const char *newname);
void manageCheckAndCreatePlaylist (const char *name, pltype_t pltype);
bool manageCreatePlaylistCopy (UIWidget *statusMsg,
    const char *oname, const char *newname);
bool managePlaylistExists (const char *name);
int manageValidateName (uientry_t *entry, void *udata);
void manageDeletePlaylist (UIWidget *statusMsg, const char *name);

/* managestats.c */
typedef struct managestats managestats_t;

managestats_t *manageStatsInit (conn_t *conn, musicdb_t *musicdb);
void  manageStatsFree (managestats_t *managestats);
UIWidget *manageBuildUIStats (managestats_t *managestats);
void manageStatsProcessData (managestats_t *managestats, mp_musicqupdate_t *musicqupdate);

#endif /* INC_MANAGEUI_H */
