#include "config.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>

#include "bdj4intl.h"
#include "bdjopt.h"
#include "bdjvarsdf.h"
#include "conn.h"
#include "level.h"
#include "log.h"
#include "nlist.h"
#include "slist.h"
#include "tagdef.h"
#include "tmutil.h"
#include "uidance.h"
#include "uifavorite.h"
#include "uigenre.h"
#include "uilevel.h"
#include "uirating.h"
#include "uistatus.h"
#include "uisong.h"
#include "uisongsel.h"
#include "uisongedit.h"
#include "ui.h"

enum {
  SONGEDIT_CHK_NONE,
  SONGEDIT_CHK_NUM,
  SONGEDIT_CHK_DOUBLE,
  SONGEDIT_CHK_STR,
};

typedef struct {
  int         tagkey;
  union {
    uientry_t     *entry;
    uispinbox_t   *spinbox;
    UIWidget      uiwidget;
    uidance_t     *uidance;
    uifavorite_t  *uifavorite;
    uigenre_t     *uigenre;
    uilevel_t     *uilevel;
    uirating_t    *uirating;
    uistatus_t    *uistatus;
  };
  uichgind_t  *chgind;
  UIWidget    display;
  UICallback  callback;
  bool        lastchanged : 1;
  bool        changed : 1;
} uisongedititem_t;

enum {
  UISONGEDIT_CB_FIRST,
  UISONGEDIT_CB_NEXT,
  UISONGEDIT_CB_PREVIOUS,
  UISONGEDIT_CB_PLAY,
  UISONGEDIT_CB_MAX,
};

typedef struct {
  UIWidget            *parentwin;
  UIWidget            vbox;
  UIWidget            filedisp;
  UIWidget            sgentry;
  UIWidget            sgsbint;
  UIWidget            sgsbtime;
  UIWidget            sgscale;
  UIWidget            sgscaledisp;
  UICallback          callbacks [UISONGEDIT_CB_MAX];
  level_t             *levels;
  song_t              *song;
  int                 itemalloccount;
  int                 itemcount;
  uisongedititem_t    *items;
  int                 changed;
} uisongeditgtk_t;

static void uisongeditAddDisplay (uisongedit_t *songedit, UIWidget *col, UIWidget *sg, int dispsel);
static void uisongeditAddItem (uisongedit_t *uisongedit, UIWidget *hbox, UIWidget *sg, int tagkey);
static void uisongeditAddEntry (uisongedit_t *uisongedit, UIWidget *hbox, int tagkey);
static void uisongeditAddSpinboxInt (uisongedit_t *uisongedit, UIWidget *hbox, int tagkey);
static void uisongeditAddLabel (uisongedit_t *uisongedit, UIWidget *hbox, int tagkey);
static void uisongeditAddSpinboxTime (uisongedit_t *uisongedit, UIWidget *hbox, int tagkey);
static void uisongeditAddScale (uisongedit_t *uisongedit, UIWidget *hbox, int tagkey);
static bool uisongeditScaleDisplayCallback (void *udata, double value);

void
uisongeditUIInit (uisongedit_t *uisongedit)
{
  uisongeditgtk_t  *uiw;

  uiw = malloc (sizeof (uisongeditgtk_t));
  uiw->parentwin = NULL;
  uiw->itemalloccount = 0;
  uiw->itemcount = 0;
  uiw->items = NULL;
  uiw->changed = 0;
  uiw->levels = bdjvarsdfGet (BDJVDF_LEVELS);
  uiutilsUIWidgetInit (&uiw->vbox);
  uiCreateSizeGroupHoriz (&uiw->sgentry);
  uiCreateSizeGroupHoriz (&uiw->sgsbint);
  uiCreateSizeGroupHoriz (&uiw->sgsbtime);
  uiCreateSizeGroupHoriz (&uiw->sgscale);
  uiCreateSizeGroupHoriz (&uiw->sgscaledisp);

  uisongedit->uiWidgetData = uiw;
}

void
uisongeditUIFree (uisongedit_t *uisongedit)
{
  uisongeditgtk_t *uiw;

  if (uisongedit == NULL) {
    return;
  }

  uiw = uisongedit->uiWidgetData;
  if (uiw != NULL) {
    for (int count = 0; count < uiw->itemcount; ++count) {
      int   tagkey;

      uichgindFree (uiw->items [count].chgind);

      tagkey = uiw->items [count].tagkey;

      switch (tagdefs [tagkey].editType) {
        case ET_ENTRY: {
          if (uiw->items [count].entry != NULL) {
            uiEntryFree (uiw->items [count].entry);
          }
          break;
        }
        case ET_COMBOBOX: {
          if (tagkey == TAG_DANCE) {
            uidanceFree (uiw->items [count].uidance);
          }
          if (tagkey == TAG_GENRE) {
            uigenreFree (uiw->items [count].uigenre);
          }
          break;
        }
        case ET_SPINBOX_TEXT: {
          if (tagkey == TAG_FAVORITE) {
            uifavoriteFree (uiw->items [count].uifavorite);
          }
          if (tagkey == TAG_DANCELEVEL) {
            uilevelFree (uiw->items [count].uilevel);
          }
          if (tagkey == TAG_DANCERATING) {
            uiratingFree (uiw->items [count].uirating);
          }
          if (tagkey == TAG_STATUS) {
            uistatusFree (uiw->items [count].uistatus);
          }
          break;
        }
        case ET_SPINBOX: {
          break;
        }
        case ET_SPINBOX_TIME: {
          if (uiw->items [count].spinbox != NULL) {
            uiSpinboxTimeFree (uiw->items [count].spinbox);
          }
          break;
        }
        case ET_NA:
        case ET_SCALE:
        case ET_LABEL: {
          break;
        }
      }
    }

    if (uiw->items != NULL) {
      free (uiw->items);
    }
    free (uiw);
    uisongedit->uiWidgetData = NULL;
  }
}

UIWidget *
uisongeditBuildUI (uisongsel_t *uisongsel, uisongedit_t *uisongedit,
    UIWidget *parentwin)
{
  uisongeditgtk_t   *uiw;
  UIWidget          hbox;
  UIWidget          col;
  UIWidget          sg;
  UIWidget          uiwidget;

  logProcBegin (LOG_PROC, "uisongeditBuildUI");

  uiw = uisongedit->uiWidgetData;
  uiw->parentwin = parentwin;

  uiCreateVertBox (&uiw->vbox);
  uiWidgetExpandHoriz (&uiw->vbox);

  uiCreateHorizBox (&hbox);
  uiWidgetExpandHoriz (&hbox);
  uiWidgetAlignHorizFill (&hbox);
  uiBoxPackStart (&uiw->vbox, &hbox);

  uiutilsUICallbackInit (&uiw->callbacks [UISONGEDIT_CB_FIRST],
      uisongselFirstSelection, uisongsel);
  uiCreateButton (&uiwidget, &uiw->callbacks [UISONGEDIT_CB_FIRST],
      /* CONTEXT: song editor : first song */
      _("First"), NULL);
  uiBoxPackStart (&hbox, &uiwidget);

  uiutilsUICallbackInit (&uiw->callbacks [UISONGEDIT_CB_PREVIOUS],
      uisongselPreviousSelection, uisongsel);
  uiCreateButton (&uiwidget, &uiw->callbacks [UISONGEDIT_CB_PREVIOUS],
      /* CONTEXT: song editor : previous song */
      _("Previous"), NULL);
  uiBoxPackStart (&hbox, &uiwidget);

  uiutilsUICallbackInit (&uiw->callbacks [UISONGEDIT_CB_NEXT],
      uisongselNextSelection, uisongsel);
  uiCreateButton (&uiwidget, &uiw->callbacks [UISONGEDIT_CB_NEXT],
      /* CONTEXT: song editor : next song */
      _("Next"), NULL);
  uiBoxPackStart (&hbox, &uiwidget);

  uiutilsUICallbackInit (&uiw->callbacks [UISONGEDIT_CB_PLAY],
      uisongselQueueProcessPlayCallback, uisongsel);
  uiCreateButton (&uiwidget, &uiw->callbacks [UISONGEDIT_CB_PLAY],
      /* CONTEXT: song editor : play song */
      _("Play"), NULL);
  uiBoxPackStart (&hbox, &uiwidget);

  uiCreateButton (&uiwidget, NULL, _("Save"), NULL);
  uiBoxPackEnd (&hbox, &uiwidget);
  uiWidgetDisable (&uiwidget);

  uiCreateHorizBox (&hbox);
  uiWidgetExpandHoriz (&hbox);
  uiWidgetAlignHorizFill (&hbox);
  uiBoxPackStart (&uiw->vbox, &hbox);

  /* CONTEXT: the audio file */
  uiCreateColonLabel (&uiwidget, _("File"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiWidgetDisable (&uiwidget);

  uiCreateLabel (&uiwidget, "");
  uiLabelEllipsizeOn (&uiwidget);
  uiBoxPackStart (&hbox, &uiwidget);
  uiutilsUIWidgetCopy (&uiw->filedisp, &uiwidget);
  uiLabelDarkenColor (&uiw->filedisp, bdjoptGetStr (OPT_P_UI_ACCENT_COL));
  uiLabelSetSelectable (&uiw->filedisp);

  uiCreateHorizBox (&hbox);
  uiWidgetExpandHoriz (&hbox);
  uiWidgetAlignHorizFill (&hbox);
  uiBoxPackStartExpand (&uiw->vbox, &hbox);

  for (int i = DISP_SEL_SONGEDIT_A; i <= DISP_SEL_SONGEDIT_C; ++i) {
    uiCreateVertBox (&col);
    uiWidgetSetAllMargins (&col, uiBaseMarginSz * 4);
    uiWidgetExpandHoriz (&col);
    uiWidgetExpandVert (&col);
    uiBoxPackStartExpand (&hbox, &col);

    uiCreateSizeGroupHoriz (&sg);
    uisongeditAddDisplay (uisongedit, &col, &sg, i);
  }

  logProcEnd (LOG_PROC, "uisongeditBuildUI", "");
  return &uiw->vbox;
}

void
uisongeditLoadData (uisongedit_t *uisongedit, song_t *song)
{
  uisongeditgtk_t *uiw;
  char            *data;
  long            val;
  double          dval;

  uiw = uisongedit->uiWidgetData;
  uiw->song = song;
  uiw->changed = 0;

  data = uisongGetDisplay (song, TAG_FILE, &val, &dval);
  uiLabelSetText (&uiw->filedisp, data);

  for (int count = 0; count < uiw->itemcount; ++count) {
    int tagkey = uiw->items [count].tagkey;

    data = uisongGetDisplay (song, tagkey, &val, &dval);
    uiw->items [count].changed = false;
    uiw->items [count].lastchanged = false;

    switch (tagdefs [tagkey].editType) {
      case ET_ENTRY: {
        uiEntrySetValue (uiw->items [count].entry, "");
        if (data != NULL) {
          uiEntrySetValue (uiw->items [count].entry, data);
          free (data);
        }
        break;
      }
      case ET_COMBOBOX: {
        if (tagkey == TAG_DANCE) {
          if (val < 0) { val = 0; }
          uidanceSetValue (uiw->items [count].uidance, val);
        }
        if (tagkey == TAG_GENRE) {
          if (val < 0) { val = 0; }
          uigenreSetValue (uiw->items [count].uigenre, val);
        }
        break;
      }
      case ET_SPINBOX_TEXT: {
        if (tagkey == TAG_FAVORITE) {
          if (val < 0) { val = 0; }
          uifavoriteSetValue (uiw->items [count].uifavorite, val);
        }
        if (tagkey == TAG_DANCELEVEL) {
          if (val < 0) { val = levelGetDefaultKey (uiw->levels); }
          uilevelSetValue (uiw->items [count].uilevel, val);
        }
        if (tagkey == TAG_DANCERATING) {
          if (val < 0) { val = 0; }
          uiratingSetValue (uiw->items [count].uirating, val);
        }
        if (tagkey == TAG_STATUS) {
          if (val < 0) { val = 0; }
          uistatusSetValue (uiw->items [count].uistatus, val);
        }
        break;
      }
      case ET_SPINBOX: {
        if (data != NULL) {
          fprintf (stderr, "et_spinbox: mismatch type\n");
        }
        if (val < 0) { val = 0; }
        uiSpinboxSetValue (&uiw->items [count].uiwidget, val);
        break;
      }
      case ET_SPINBOX_TIME: {
        if (data != NULL) {
          fprintf (stderr, "et_spinbox_time: mismatch type\n");
        }
        if (val < 0) { val = 0; }
        uiSpinboxTimeSetValue (uiw->items [count].spinbox, val);
        break;
      }
      case ET_SCALE: {
        if (tagkey == TAG_SPEEDADJUSTMENT) {
          dval = (double) val;
          if (dval == 0.0) { dval = 100.0; }
        }
        if (data != NULL) {
          fprintf (stderr, "et_scale: mismatch type\n");
        }
        uiScaleSetValue (&uiw->items [count].uiwidget, dval);
        uisongeditScaleDisplayCallback (&uiw->items [count], dval);
        break;
      }
      case ET_LABEL: {
        if (data != NULL) {
          uiLabelSetText (&uiw->items [count].uiwidget, data);
          free (data);
        }
        break;
      }
      default: {
        break;
      }
    }
  }
}

void
uisongeditUIMainLoop (uisongedit_t *uisongedit)
{
  uisongeditgtk_t *uiw;
  double          dval, ndval;
  long            val, nval;
  char            *songdata;
  const char      *ndata;
  int             chkvalue;

  uiw = uisongedit->uiWidgetData;

  /* look for changed items */
  for (int count = 0; count < uiw->itemcount; ++count) {
    int tagkey = uiw->items [count].tagkey;

    songdata = uisongGetDisplay (uiw->song, tagkey, &val, &dval);
    chkvalue = SONGEDIT_CHK_NONE;

    switch (tagdefs [tagkey].editType) {
      case ET_ENTRY: {
        ndata = uiEntryGetValue (uiw->items [count].entry);
        chkvalue = SONGEDIT_CHK_STR;
        break;
      }
      case ET_COMBOBOX: {
        if (tagkey == TAG_DANCE) {
          nval = uidanceGetValue (uiw->items [count].uidance);
          chkvalue = SONGEDIT_CHK_NUM;
        }
        if (tagkey == TAG_GENRE) {
          nval = uigenreGetValue (uiw->items [count].uigenre);
          chkvalue = SONGEDIT_CHK_NUM;
        }
        break;
      }
      case ET_SPINBOX_TEXT: {
        if (tagkey == TAG_FAVORITE) {
          nval = uifavoriteGetValue (uiw->items [count].uifavorite);
          chkvalue = SONGEDIT_CHK_NUM;
        }
        if (tagkey == TAG_DANCELEVEL) {
          nval = uilevelGetValue (uiw->items [count].uilevel);
          chkvalue = SONGEDIT_CHK_NUM;
        }
        if (tagkey == TAG_DANCERATING) {
          nval = uiratingGetValue (uiw->items [count].uirating);
          chkvalue = SONGEDIT_CHK_NUM;
        }
        if (tagkey == TAG_STATUS) {
          nval = uistatusGetValue (uiw->items [count].uistatus);
          chkvalue = SONGEDIT_CHK_NUM;
        }
        break;
      }
      case ET_SPINBOX: {
        nval = uiSpinboxGetValue (&uiw->items [count].uiwidget);
        chkvalue = SONGEDIT_CHK_NUM;
        break;
      }
      case ET_SPINBOX_TIME: {
        nval = uiSpinboxTimeGetValue (uiw->items [count].spinbox);
        chkvalue = SONGEDIT_CHK_NUM;
        break;
      }
      case ET_SCALE: {
        ndval = uiScaleGetValue (&uiw->items [count].uiwidget);
        chkvalue = SONGEDIT_CHK_DOUBLE;
        break;
      }
      default: {
        chkvalue = SONGEDIT_CHK_NONE;
        break;
      }
    }

    if (chkvalue == SONGEDIT_CHK_STR) {
      if ((ndata != NULL && songdata == NULL) ||
          strcmp (songdata, ndata) != 0) {
        uiw->items [count].changed = true;
      } else {
        if (uiw->items [count].changed) {
          uiw->items [count].changed = false;
        }
      }
    }

    if (chkvalue == SONGEDIT_CHK_NUM) {
      int   rcdisc, rctrk;

      rcdisc = (tagkey == TAG_DISCNUMBER && val == 0.0 && nval == 1.0);
      rctrk = (tagkey == TAG_TRACKNUMBER && val == 0.0 && nval == 1.0);
      if (! rcdisc && ! rctrk && nval != val) {
        uiw->items [count].changed = true;
      } else {
        if (uiw->items [count].changed) {
          uiw->items [count].changed = false;
        }
      }
    }

    if (chkvalue == SONGEDIT_CHK_DOUBLE) {
      int   rc;

      /* for the speed adjustment, 0.0 (no setting) is changed to 100.0 */
      rc = (tagkey == TAG_SPEEDADJUSTMENT && ndval == 100.0 && dval == 0.0);
      if (! rc && ndval != dval) {
        uiw->items [count].changed = true;
      } else {
        if (uiw->items [count].changed) {
          uiw->items [count].changed = false;
        }
      }
    }

    if (uiw->items [count].changed != uiw->items [count].lastchanged) {
      if (uiw->items [count].changed) {
        uichgindMarkChanged (uiw->items [count].chgind);
        uiw->changed += 1;
      } else {
        uichgindMarkNormal (uiw->items [count].chgind);
        uiw->changed -= 1;
      }
      uiw->items [count].lastchanged = uiw->items [count].changed;
    }
  }

  return;
}

/* internal routines */

static void
uisongeditAddDisplay (uisongedit_t *uisongedit, UIWidget *col, UIWidget *sg, int dispsel)
{
  slist_t         *sellist;
  UIWidget        hbox;
  char            *keystr;
  slistidx_t      dsiteridx;
  uisongeditgtk_t *uiw;

  uiw = uisongedit->uiWidgetData;
  sellist = dispselGetList (uisongedit->dispsel, dispsel);

  slistStartIterator (sellist, &dsiteridx);
  while ((keystr = slistIterateKey (sellist, &dsiteridx)) != NULL) {
    int   tagkey;

    tagkey = slistGetNum (sellist, keystr);

    if (tagkey >= TAG_KEY_MAX) {
      continue;
    }
    if (tagdefs [tagkey].editType != ET_LABEL &&
        ! tagdefs [tagkey].isEditable) {
      continue;
    }

    uiCreateHorizBox (&hbox);
    uiWidgetExpandHoriz (&hbox);
    uiBoxPackStart (col, &hbox);

    if (uiw->itemcount >= uiw->itemalloccount) {
      uiw->itemalloccount += 10;
      uiw->items = realloc (uiw->items,
          sizeof (uisongedititem_t) * uiw->itemalloccount);
    }

    uisongeditAddItem (uisongedit, &hbox, sg, tagkey);

    uiw->items [uiw->itemcount].tagkey = tagkey;

    ++uiw->itemcount;
  }
}

static void
uisongeditAddItem (uisongedit_t *uisongedit, UIWidget *hbox, UIWidget *sg, int tagkey)
{
  UIWidget        uiwidget;
  uisongeditgtk_t *uiw;

  uiw = uisongedit->uiWidgetData;

  uiw->items [uiw->itemcount].chgind = uiCreateChangeIndicator (hbox);
  uiw->items [uiw->itemcount].changed = false;
  uiw->items [uiw->itemcount].lastchanged = false;

  uiCreateColonLabel (&uiwidget, tagdefs [tagkey].displayname);
  uiBoxPackStart (hbox, &uiwidget);
  uiSizeGroupAdd (sg, &uiwidget);

  switch (tagdefs [tagkey].editType) {
    case ET_ENTRY: {
      uisongeditAddEntry (uisongedit, hbox, tagkey);
      break;
    }
    case ET_COMBOBOX: {
      if (tagkey == TAG_DANCE) {
        uiw->items [uiw->itemcount].uidance =
            uidanceDropDownCreate (hbox, uiw->parentwin,
            false, "", UIDANCE_PACK_START);
      }
      if (tagkey == TAG_GENRE) {
        uiw->items [uiw->itemcount].uigenre =
            uigenreDropDownCreate (hbox, uiw->parentwin, false);
      }
      break;
    }
    case ET_SPINBOX_TEXT: {
      if (tagkey == TAG_FAVORITE) {
        uiw->items [uiw->itemcount].uifavorite =
            uifavoriteSpinboxCreate (hbox);
      }
      if (tagkey == TAG_DANCELEVEL) {
        uiw->items [uiw->itemcount].uilevel =
            uilevelSpinboxCreate (hbox, FALSE);
      }
      if (tagkey == TAG_DANCERATING) {
        uiw->items [uiw->itemcount].uirating =
            uiratingSpinboxCreate (hbox, FALSE);
      }
      if (tagkey == TAG_STATUS) {
        uiw->items [uiw->itemcount].uistatus =
            uistatusSpinboxCreate (hbox, FALSE);
      }
      break;
    }
    case ET_SPINBOX: {
      uisongeditAddSpinboxInt (uisongedit, hbox, tagkey);
      break;
    }
    case ET_SPINBOX_TIME: {
      uisongeditAddSpinboxTime (uisongedit, hbox, tagkey);
      break;
    }
    case ET_SCALE: {
      uisongeditAddScale (uisongedit, hbox, tagkey);
      break;
    }
    case ET_LABEL: {
      uisongeditAddLabel (uisongedit, hbox, tagkey);
      break;
    }
    case ET_NA: {
      break;
    }
  }
}

static void
uisongeditAddEntry (uisongedit_t *uisongedit, UIWidget *hbox, int tagkey)
{
  uientry_t       *entryp;
  UIWidget        *uiwidgetp;
  uisongeditgtk_t *uiw;

  uiw = uisongedit->uiWidgetData;
  entryp = uiEntryInit (tagdefs [tagkey].editWidth, 100);
  uiw->items [uiw->itemcount].entry = entryp;
  uiEntryCreate (entryp);
  uiwidgetp = uiEntryGetUIWidget (entryp);
  uiWidgetAlignHorizFill (uiwidgetp);
  uiSizeGroupAdd (&uiw->sgentry, uiwidgetp);
  uiBoxPackStartExpand (hbox, uiwidgetp);
}

static void
uisongeditAddSpinboxInt (uisongedit_t *uisongedit, UIWidget *hbox, int tagkey)
{
  UIWidget        *uiwidgetp;
  uisongeditgtk_t *uiw;

  uiw = uisongedit->uiWidgetData;
  uiwidgetp = &uiw->items [uiw->itemcount].uiwidget;
  uiSpinboxIntCreate (uiwidgetp);
  if (tagkey == TAG_BPM) {
    uiSpinboxSet (uiwidgetp, 0.0, 400.0);
  }
  if (tagkey == TAG_TRACKNUMBER || tagkey == TAG_DISCNUMBER) {
    uiSpinboxSet (uiwidgetp, 1.0, 300.0);
  }
  uiSizeGroupAdd (&uiw->sgsbint, uiwidgetp);
  uiBoxPackStart (hbox, uiwidgetp);
}

static void
uisongeditAddLabel (uisongedit_t *uisongedit, UIWidget *hbox, int tagkey)
{
  UIWidget        *uiwidgetp;
  uisongeditgtk_t *uiw;

  uiw = uisongedit->uiWidgetData;
  uiwidgetp = &uiw->items [uiw->itemcount].uiwidget;
  uiCreateLabel (uiwidgetp, "");
  uiLabelEllipsizeOn (uiwidgetp);
  uiSizeGroupAdd (&uiw->sgentry, uiwidgetp);
  uiBoxPackStartExpand (hbox, uiwidgetp);
}

static void
uisongeditAddSpinboxTime (uisongedit_t *uisongedit, UIWidget *hbox, int tagkey)
{
  uispinbox_t     *sbp;
  uisongeditgtk_t *uiw;
  UIWidget        *uiwidgetp;

  uiw = uisongedit->uiWidgetData;
  sbp = uiSpinboxTimeInit (SB_TIME_PRECISE);
  uiw->items [uiw->itemcount].spinbox = sbp;
  uiSpinboxTimeCreate (sbp, uisongedit, NULL);
  uiSpinboxTimeSetValue (sbp, 0);
  uiwidgetp = uiSpinboxGetUIWidget (sbp);
  uiSizeGroupAdd (&uiw->sgsbtime, uiwidgetp);
  uiBoxPackStart (hbox, uiwidgetp);
}

static void
uisongeditAddScale (uisongedit_t *uisongedit, UIWidget *hbox, int tagkey)
{
  uisongeditgtk_t *uiw;
  UIWidget        *uiwidgetp;
  double          lower, upper;
  int             digits;

  uiw = uisongedit->uiWidgetData;
  uiwidgetp = &uiw->items [uiw->itemcount].uiwidget;
  lower = 70.0;
  upper = 130.0;
  digits = 0;
  if (tagkey == TAG_VOLUMEADJUSTPERC) {
    lower = -50.0;
    upper = 50.0;
    digits = 1;
  }
  uiCreateScale (uiwidgetp, lower, upper, 1.0, 5.0, 0.0, digits);
  uiutilsUICallbackDoubleInit (&uiw->items [uiw->itemcount].callback,
      uisongeditScaleDisplayCallback, &uiw->items [uiw->itemcount]);
  uiScaleSetCallback (uiwidgetp, &uiw->items [uiw->itemcount].callback);
  uiSizeGroupAdd (&uiw->sgscale, uiwidgetp);
  uiBoxPackStart (hbox, uiwidgetp);

  uiwidgetp = &uiw->items [uiw->itemcount].display;
  uiCreateLabel (uiwidgetp, "100%");
  uiLabelAlignEnd (uiwidgetp);
  uiSizeGroupAdd (&uiw->sgscaledisp, uiwidgetp);
  uiBoxPackStart (hbox, uiwidgetp);
}

static bool
uisongeditScaleDisplayCallback (void *udata, double value)
{
  uisongedititem_t  *item = udata;
  char              tbuff [40];
  int               digits;

  digits = uiScaleGetDigits (&item->uiwidget);
  snprintf (tbuff, sizeof (tbuff), "%.*f%%", digits, value);
  uiLabelSetText (&item->display, tbuff);
  return UICB_CONT;
}
