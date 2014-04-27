#ifndef __VO_P96PIP_H__
#define __VO_P96PIP_H__

#ifndef WORDS_BIGENDIAN
#define WORDS_BIGENDIAN 1
#endif

static vo_info_t info =
{
    "Picasso96 video output (PIP)",
    "p96_pip",
    "Joerg Strohmayer and Andrea Palmate'",
    ""
};

LIBVO_EXTERN(p96_pip)

// To make gcc happy
extern void mplayer_put_key(int code);
extern int vidmode;
extern void put_command0(int cmd);
extern void put_icommand1(int cmd,int v);
extern void put_icommand2(int cmd,int v,int v2);
extern void put_fcommand2(int cmd,float value,int v);
extern long rxid_get_time_pos();
extern long rxid_get_time_length();
extern int abs_seek_pos;
extern float rel_seek_secs;

/* ARexx */
extern struct ArexxHandle *rxHandler; // = NULL;
/* ARexx */

extern mp_cmd_t mp_cmds[]; //static
extern char *EXTPATTERN;
extern uint32 AppID;

// Some proto for our private func
static void (*vo_draw_alpha_func)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);

static struct Window *My_Window = NULL;
static struct Screen *My_Screen = NULL;

static struct AppWindow *AppWin;
extern struct MsgPort   *AppPort;
static struct MsgPort	*NotSerAppPort;

static struct RastPort *rp = NULL;
static struct MsgPort *UserMsg = NULL;

// not OS specific
static uint32_t     image_width;            // well no comment
static uint32_t     image_height;
static uint32_t     window_width;           // width and height on the window
static uint32_t     window_height;          // can be different from the image
static int			out_width = 0, out_height = 0; //those variables keeps the gui size when switching from a size to another
static int			keep_width = 0, keep_height = 0; //those variables keeps the original movie size
static float 		ratio = 1;				//image ratio
static uint32_t     image_format;
static uint32_t     old_d_width=0, old_d_height=0, old_d_posx=0, old_d_posy=0;

static uint32_t     is_ontop = 0, loop_on = 0;
static uint32_t		choosing = 0;

struct MsgPort 		*_port;
uint32 				 _timerSig;
struct TimeRequest 	*_timerio;
struct TimerIFace 	*ITimer;
#define RESET_TIME 1000 * 3000
static uint32_t		reset_time = RESET_TIME;

struct DiskObject	*DIcon;
struct AppIcon 		*MPLAppIcon = NULL;

static UWORD *EmptyPointer = NULL;

static BOOL FirstTime = TRUE;				//used when a file is loaded for the first time
static BOOL hasGUI = FALSE;
static BOOL isPaused = FALSE;
static BOOL hasNotificationSystem = FALSE;
//static BOOL Closing = TRUE;
static BOOL Stopped = FALSE;
static Object		*IconifyImage = NULL;
static Object		*IconifyGadget = NULL;

enum gadgetIDs
{
	GID_MAIN,
	GID_Iconify,
    GID_Back,
    GID_Forward,
    GID_Play,
    GID_Stop,
    GID_Eject,
    GID_FullScreen,
    GID_Volume,
    GID_Pause,
    GID_Time,
    GID_END
};

struct pmpMessage
{
        STRPTR about;
        ULONG type;
        ULONG image;
};

Object *OBJ[GID_END];
Object *WinLayout;
Object *Space_Object;
struct Gadget *ToolBarObject;

Object *image[];

static uint32_t     win_left;
static uint32_t     win_top;

static uint32_t     g_in_format, g_out_format;

static BOOL         isStarted       = FALSE;

/****************** MENU DEFINITION *****************/
    static struct Menu *menu;
    static struct NewMenu nm[] =
    {
      /* Type, Label, CommKey, Flags, MutualExclude, UserData */

      { NM_TITLE, (STRPTR)"MPlayer",                    NULL,   0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Load File..",            (STRPTR)"L",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Open DVD",            	(STRPTR)"D",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Open VCD",            	(STRPTR)"V",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Open Network",          	(STRPTR)"N",    0, 0L, NULL },
          { NM_ITEM,  NM_BARLABEL,              NULL,	0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"About MPlayer",        	(STRPTR)"A",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Quit",                   (STRPTR)"Q",    0, 0L, NULL },

      { NM_TITLE, (STRPTR)"Play",                       NULL,   0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Play/Pause",             (STRPTR)"P",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Stop",                   (STRPTR)"S",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Loop",                   (STRPTR)"L",    MENUTOGGLE|CHECKIT,  0L, NULL },

      { NM_TITLE, (STRPTR)"Video Options",              NULL,   0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Stay on Top",            NULL,   MENUTOGGLE|CHECKIT , 0L, NULL },
          { NM_ITEM,  (STRPTR)"Cycle OSD",              (STRPTR)"O",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Fullscreen",             (STRPTR)"F",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Screenshot",             NULL,   0, 0L, NULL },
          { NM_ITEM,  NM_BARLABEL,              NULL,	0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Normal Dimension",       (STRPTR)"1",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Doubled Dimension",      (STRPTR)"2",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Screen Dimension",       (STRPTR)"3",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Half Dimension",         (STRPTR)"4",    0, 0L, NULL },
          { NM_ITEM,  NM_BARLABEL,              NULL,	0, 0L, NULL },
/*23*/    { NM_ITEM,  (STRPTR)"Aspect Ratio",       	NULL,   0, 0L, NULL },
		  { NM_SUB,   (STRPTR)"Original",      			NULL,   MENUTOGGLE|CHECKIT|CHECKED, ~1, NULL },
          { NM_SUB,   (STRPTR)"16:10",      			NULL,   MENUTOGGLE|CHECKIT, ~2, NULL },
          { NM_SUB,   (STRPTR)"16:9",      				NULL,   MENUTOGGLE|CHECKIT, ~4, NULL },
          { NM_SUB,   (STRPTR)"1.85:1",      			NULL,   MENUTOGGLE|CHECKIT, ~8, NULL },
          { NM_SUB,   (STRPTR)"2.21:1",      			NULL,   MENUTOGGLE|CHECKIT, ~16, NULL },
          { NM_SUB,   (STRPTR)"2.35:1",      			NULL,   MENUTOGGLE|CHECKIT, ~32, NULL },
          { NM_SUB,   (STRPTR)"2.39:1",      			NULL,   MENUTOGGLE|CHECKIT, ~64, NULL },
          { NM_SUB,   (STRPTR)"5:3",      				NULL,   MENUTOGGLE|CHECKIT, ~128, NULL },
          { NM_SUB,   (STRPTR)"4:3",      				NULL,   MENUTOGGLE|CHECKIT, ~256, NULL },
          { NM_SUB,   (STRPTR)"5:4",      				NULL,   MENUTOGGLE|CHECKIT, ~512, NULL },
          { NM_SUB,   (STRPTR)"1:1",      				NULL,   MENUTOGGLE|CHECKIT, ~1024, NULL },

      { NM_TITLE, (STRPTR)"Audio Options",              NULL,   0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Mute",                   NULL,   MENUTOGGLE|CHECKIT , 0, NULL },
          { NM_ITEM,  (STRPTR)"Volume Up"     ,         (STRPTR)"+",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Volume Down",            (STRPTR)"-",    0, 0L, NULL },
      { NM_END,   NULL,                         NULL,   0, 0L, NULL }
    };

/****************** END MENU DEFINITION *****************/

static struct Library   *LayersBase;
static struct Library   *GadToolsBase;
static struct Library   *P96Base;
static struct LayersIFace   *ILayers;
static struct GadToolsIFace *IGadTools;
static struct P96IFace      *IP96;
static struct Library 			 *ApplicationBase;
static struct ApplicationIFace  *IApplication;

#define NOBORDER            0
#define TINYBORDER          1
#define ALWAYSBORDER        2
static ULONG p96pip_BorderMode = ALWAYSBORDER;
//static ULONG old_p96_BorderMode = ALWAYSBORDER;

/* This enum allow to change size of window at runtime */
enum WINDOW_SIZE
{
	x1 = 1,			/* Normal Size  */
	x2,				/* Doubled Size */
	xF,				/* Screen Size  */
	xH,				/* Half Size    */
	AR_ORIGINAL,
	AR_16_10,
	AR_16_9,
	AR_185_1,
	AR_221_1,
	AR_235_1,
	AR_239_1,
	AR_5_3,
	AR_4_3,
	AR_5_4,
	AR_1_1,
};
static int AspectRatio = x1;

/* This enum allow to higer or lower the volume */
enum VOLUME_DIRECTION
{
	VOL_UP = 1,
	VOL_DOWN = -1
};

#define NUMBUFFERS			1

static char *window_title = NULL;
static char *current_filename = NULL;
// Double-click stuff
ULONG secs, mics, secs2, mics2;
// Timer for the pointer...
ULONG p_mics1, p_mics2, p_secs1, p_secs2;
static BOOL mouse_hidden=FALSE;

static struct Gadget MyDragGadget =
{
    NULL,
    0,0,                                             // Pos
    -16,-16,                                             // Size
    GFLG_RELWIDTH | GFLG_RELHEIGHT | GFLG_GADGHNONE, // Flags
    0,                                               // Activation
    GTYP_WDRAGGING,                                  // Type
    NULL,                                            // GadgetRender
    NULL,                                            // SelectRender
    NULL,                                            // GadgetText
    0L,                                              // Obsolete
    NULL,                                            // SpecialInfo
    0,                                               // Gadget ID
    NULL                                             // UserData
};


/* Function prototypes */

static int32 p96pip_PlayFile(const char *FileName);
static void p96pip_NotifyServer(STRPTR FileName);
void p96pip_RemoveAppPort(void);
static void p96pip_appw_events(void);
static BOOL p96pip_GiveArg(const char *arg);
static void p96pip_CloseOSLibs(void);
static BOOL p96pip_OpenOSLibs(void);
static void p96pip_ChangeWindowSize(int mode);
static void p96pip_SetVolume(int direction);
static UBYTE *p96pip_LoadFile(const char *StartingDir);
static BOOL p96pip_CheckEvents(struct Window *My_Window, uint32_t *window_height, uint32_t *window_width,  uint32_t *window_left, uint32_t *window_top );
static char *p96pip_GetWindowTitle(void);
static void draw_alpha_yv12(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);
static ULONG GoFullScreen(void);
static ULONG Open_PIPWindow(void);
static ULONG Open_FullScreen(void);
static void FreeGfx(void);
static int32 p96pip_PrintMsgProc(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase);
static void p96pip_PrintMsg(CONST_STRPTR text,int REQ_TYPE, int REQ_IMAGE);
static void p96pip_ShowAbout(void);
static void p96pip_OpenNetwork(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED);
static void p96pip_OpenDVD(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED);
static void p96pip_OpenVCD(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED);
void p96pip_ChangePauseButton(void);
static struct Gadget *p96pip_CreateWinLayout(struct Screen *screen, struct Window *window);
void p96pip_TimerReset(uint32 microDelay);
BOOL p96pip_TimerInit(void);
void p96pip_TimerExit(void);
#if 0
static void ClearSpaceArea(struct Window *window);
#endif

#endif
