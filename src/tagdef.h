#ifndef INC_TAGDEF_H
#define INC_TAGDEF_H

#include <stdbool.h>

#include "bdjstring.h"

typedef enum {
  DISP_YES,
  DISP_OPT,
  DISP_NO,
} tagdispflag_t;

typedef enum {
  ET_SCALE,
  ET_CHECKBUTTON,
  ET_COMBOBOX,
  ET_ENTRY,
  ET_NA,
  ET_DISABLED_ENTRY
} tagedittype_t;

typedef enum {
  ALIGN_START,
  ALIGN_END,
  ALIGN_CENTER,
} tagalign_t;

enum {
  TAG_TYPE_M4A,
  TAG_TYPE_MP3,
  TAG_TYPE_WMA,
  TAG_TYPE_MAX,
};

typedef struct {
  char                *tag;
  char                *displayname;
  char                *audiotags [TAG_TYPE_MAX];
  unsigned int        defaultEditOrder;
  unsigned int        editIndex;
  unsigned int        editWidth;
  tagalign_t          listingAnchor;
  tagalign_t          songlistAnchor;
  tagedittype_t       editType;
  tagdispflag_t       audioiddispflag;
  bool                listingDisplay : 1;
  bool                songListDisplay : 1;
  bool                isBdjTag : 1;
  bool                isNormTag : 1;
  bool                albumEdit : 1;
  bool                allEdit : 1;
  bool                isEditable : 1;
  bool                textSearchable : 1;
  bool                isOrgTag : 1;
} tagdef_t;

typedef enum {
  TAG_ADJUSTFLAGS,
  TAG_AFMODTIME,
  TAG_ALBUM,                  //
  TAG_ALBUMARTIST,            //
  TAG_ARTIST,                 //
  TAG_AUTOORGFLAG,
  TAG_BPM,                    //
  TAG_COMPOSER,
  TAG_CONDUCTOR,
  TAG_DANCE,                  //
  TAG_DANCELEVEL,             //
  TAG_DANCERATING,            //
  TAG_DATE,
  TAG_DBADDDATE,              //
  TAG_DBIDX,                  // not saved
  TAG_DISCNUMBER,
  TAG_DISCTOTAL,
  TAG_DURATION,               //
  TAG_FAVORITE,
  TAG_FILE,                   //
  TAG_GENRE,
  TAG_KEYWORD,                //
  TAG_MQDISPLAY,              //
  TAG_MUSICBRAINZ_TRACKID,
  TAG_NOTES,
  TAG_SAMESONG,
  TAG_SONGEND,                //
  TAG_SONGSTART,              //
  TAG_SPEEDADJUSTMENT,        //
  TAG_STATUS,                 //
  TAG_TAGS,
  TAG_TITLE,
  TAG_TRACKNUMBER,
  TAG_TRACKTOTAL,
  TAG_UPDATEFLAG,
  TAG_UPDATETIME,
  TAG_VOLUMEADJUSTPERC,       //
  TAG_WRITETIME,
  TAG_RRN,                    //
  TAG_MAX_KEY,
} tagdefkey_t;

extern tagdef_t tagdefs [TAG_MAX_KEY];

void        tagdefInit (void);
void        tagdefCleanup (void);
tagdefkey_t tagdefLookup (char *str);

#endif /* INC_TAGDEF_H */
