/*
 *  vo_p96_pip.c
 *  VO module for MPlayer AmigaOS4
 *  using p96 PIP
 *  Writen by Jörg strohmayer
 *  modified by Kjetil hvalstrand
*/

#define USE_VMEM64	1


#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "config.h"

#include "../sub/osd.h"
#include "../sub/sub.h"

#include "mp_msg.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "../mplayer.h"
#include "../libmpcodecs/vf_scale.h"
#include "../input/input.h"

#include <version.h>

// OS specific
#include <libraries/keymap.h>
#include <osdep/keycodes.h>

#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <proto/icon.h>
#include <proto/requester.h>
#include <proto/layout.h>
#include <proto/utility.h>

#include <proto/Picasso96API.h>
#include <exec/types.h>

#include <graphics/rpattr.h>
#include <dos/dostags.h>
#include <dos/dos.h>


#include "aspect.h"
#include "input/input.h"
#include "../libmpdemux/demuxer.h"

#include <assert.h>
#undef CONFIG_GUI

#include "cgx_common.h"

#define PUBLIC_SCREEN 0

static vo_info_t info =
{
	"Picasso96 overlay",
	"p96_pip",
	"Jörg strohmayer",
	"AmigaOS4 rules da world !"
};


LIBVO_EXTERN(p96_pip)

#define MIN_WIDTH 290
#define MIN_HEIGHT 217

// Some proto for our private func
static void (*vo_draw_alpha_func)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);

extern BOOL gfx_CheckEvents(struct Screen *My_Screen, struct Window *My_Window, uint32_t *window_height, uint32_t *window_width,uint32_t *window_left, uint32_t *window_top );

static void FreeGfx(void);

struct Screen *My_Screen;
struct Window *My_Window;

extern uint32		amiga_image_width;
extern uint32		amiga_image_height;
extern float		amiga_aspect_ratio;

ULONG window_width = 0;
ULONG window_height =0;
static uint32_t   screen_width;           // Indicates the size of the screen in full screen
static uint32_t   screen_height;          // Only use for triple buffering


static BOOL FirstTime = TRUE;

extern uint32_t is_fullscreen;

ULONG win_top = 0;
ULONG win_left = 0;
ULONG old_d_width = 0;
ULONG old_d_height = 0;


// Found in cgx_common
extern UWORD *EmptyPointer;
extern BOOL mouse_hidden;


ULONG g_in_format;
ULONG g_out_format;
BOOL Stopped = FALSE;
BOOL keep_width = FALSE;
BOOL keep_height = FALSE;
BOOL is_ontop= TRUE;

ULONG amiga_image_format;

float ratio;

struct RastPort *rp;
struct MsgPort *UserMsg;

extern APTR window_mx;

struct Window *open_pip_window(BOOL is_full_screen, int width, int height);

static int numbuffers=3;

/******************************** DRAW ALPHA ******************************************/

/**
 * draw_alpha is used for osd and subtitle display.
 *
 **/
static void draw_alpha_yv12(int x0, int y0, int w, int h, unsigned char* src, unsigned char *srca, int stride)
{
	register int x;
	register UWORD *dstbase;
	register UWORD *dst;
	struct BitMap *bm;
    struct RenderInfo ri;
    int lock_h;

	if (!w || !h)
		return;

    	p96PIP_GetTags(My_Window, P96PIP_SourceBitMap   , (ULONG)&bm ,  TAG_DONE );

    if (! (lock_h = p96LockBitMap(bm, (uint8 *)&ri, sizeof(struct RenderInfo))))
    {
        // Unable to lock the BitMap -> do nothing
        return;
    }

    dstbase = (UWORD *)(ri.Memory + ( ( (y0 * amiga_image_width) + x0) * 2));

    do
    {
        x = 0;
        do
        {
            dst = dstbase;
            if (srca[x])
            {
                __asm__ __volatile__(
                    "li %%r4,0x0080;"
                    "rlwimi %%r4,%1,8,16,23;"
                    "sth %%r4,0(%0);"
                    :
                    : "b" ( dst + x), "r" (src[x])
                    : "r4"
                );
            }
        } while (++x < w);

        src     +=   stride;
        srca    +=   stride;
        dstbase +=   amiga_image_width;

    } while (--h);

	p96UnlockBitMap(bm, lock_h);
}

/******************************** PREINIT ******************************************/
static int preinit(const char *arg)
{
    mp_msg(MSGT_VO, MSGL_INFO, "VO: [p96_pip]\n");

#ifdef CONFIG_GUI
	mygui = malloc(sizeof(*mygui));
	initguimembers();
#endif
    return 0;
}


static ULONG Open_PIPWindow(void)
{
    // Window
    ULONG ModeID = INVALID_ID;

    My_Window = NULL;

    if ( ( My_Screen = LockPubScreen ( NULL ) ) )
    {

	if (FirstTime)
	{
		gfx_center_window(My_Screen, window_width, window_height, &win_left, &win_top);
		FirstTime = FALSE;
	}

	My_Window = p96PIP_OpenTags(
			P96PIP_SourceFormat, 	RGBFB_YUV422CGX,
			P96PIP_SourceWidth,		amiga_image_width,
			P96PIP_SourceHeight,	amiga_image_height,
			P96PIP_AllowCropping,	TRUE,
			P96PIP_NumberOfBuffers, numbuffers,
			P96PIP_Relativity, PIPRel_Width | PIPRel_Height,

#ifdef CONFIG_GUI
			P96PIP_Width, (ULONG) -1,
			P96PIP_Height, (ULONG)-34,
#endif
			P96PIP_ColorKeyPen, 	0,

			WA_CustomScreen,	(ULONG) My_Screen,
			WA_ScreenTitle,		(ULONG) gfx_common_screenname,
			WA_Title,			"MPlayer for AmigaOS4 (PIP)",
			WA_Left,			win_left,
			WA_Top,				win_top,
			WA_InnerWidth,		window_width,
			WA_InnerHeight,		window_height,
			WA_MaxWidth,		~0,
			WA_MaxHeight,		~0,
			WA_MinWidth, 		MIN_WIDTH,
			WA_MinHeight, 		MIN_HEIGHT,
			WA_NewLookMenus,	TRUE,
			WA_SmartRefresh,	TRUE,
			WA_NoCareRefresh,	TRUE,
			WA_CloseGadget,		TRUE,
			WA_DepthGadget,		TRUE,
			WA_DragBar,			TRUE,
			WA_Borderless,		FALSE,
			WA_SizeGadget,		TRUE,
			WA_SizeBBottom,		TRUE,
			WA_Activate,		TRUE,
			WA_Hidden,			FALSE, //this avoid a bug into WA_StayTop when used with WA_Hidden
			WA_PubScreen,		My_Screen,
//			WA_StayTop,         (is_ontop==1) ? TRUE : FALSE,
			WA_IDCMP,			IDCMP_COMMON,
					TAG_DONE);


		if (My_Window)
		{
			/* Clearing Pointer */
			ClearPointer(My_Window);
		}

		vo_screenwidth = My_Screen->Width;
		vo_screenheight = My_Screen->Height;

		UnlockPubScreen(NULL, My_Screen);
    }

    if ( !My_Window )
    {
        mp_msg(MSGT_VO, MSGL_ERR, "Unable to open a window\n");
        return INVALID_ID;
    }

    if ( (ModeID = GetVPModeID(&My_Window->WScreen->ViewPort) ) == INVALID_ID)
    {
        return INVALID_ID;
    }

	mouse_hidden = FALSE;
	ClearPointer(My_Window);

	vo_dwidth = amiga_image_width;
	vo_dheight = amiga_image_height;

	is_fullscreen = 0;

	ScreenToFront(My_Window->WScreen);

	return ModeID;
}

static ULONG GoFullScreen(void)
{
	ULONG left = 0, top = 0;
    ULONG ModeID = INVALID_ID;
	ULONG out_width = 0;
	ULONG out_height = 0;


    if (My_Window)
    {
		p96PIP_Close(My_Window);
		My_Window = NULL;
	}

	My_Screen = OpenScreenTags ( 		NULL,
								SA_LikeWorkbench,	TRUE,
								SA_Type,			PUBLICSCREEN,
								SA_PubName, 		"MPlayer Screen",
								SA_Title, 			"MPlayer Screen",
					            SA_ShowTitle, 		FALSE,
								SA_Quiet, 			TRUE,
								TAG_DONE);

	if ( ! My_Screen )
	{
		return INVALID_ID;
	}

	/* Entering Game Mode so Notification System doesn't send messages to us.. */
//	SetApplicationAttrs(AppID, APPATTR_NeedsGameMode, TRUE, TAG_DONE);

	/* Fill the screen with black color */
	p96RectFill(&(My_Screen->RastPort), 0,0, My_Screen->Width, My_Screen->Height, 0x00000000);

	/* calculate new video size/aspect */
	aspect_save_screenres(My_Screen->Width,My_Screen->Height);

	out_width = amiga_image_width;
	out_height = amiga_image_height;

   	aspect(&out_width,&out_height,A_ZOOM);

	left=(My_Screen->Width-out_width)/2;
	top=(My_Screen->Height-out_height)/2;

	My_Window = p96PIP_OpenTags(
                      	P96PIP_SourceFormat, RGBFB_YUV422CGX,
                      	P96PIP_SourceWidth,  amiga_image_width,
                      	P96PIP_SourceHeight, amiga_image_height,
						P96PIP_NumberOfBuffers, numbuffers,

                      	WA_CustomScreen,    (ULONG) My_Screen,
                      	WA_ScreenTitle,     (ULONG) "MPlayer ",
                      	WA_SmartRefresh,    TRUE,
                      	WA_CloseGadget,     FALSE,
                      	WA_DepthGadget,     FALSE,
                      	WA_DragBar,         FALSE,
                      	WA_Borderless,      TRUE,
                      	WA_SizeGadget,      FALSE,
                      	WA_Activate,        TRUE,
                      	WA_StayTop,		  	TRUE,
						WA_DropShadows,		FALSE,
                      	WA_Flags,		WFLG_RMBTRAP,
                      	WA_IDCMP,		IDCMP_COMMON, 

                      	TAG_DONE);

//	hasGUI = FALSE;

	if ( !My_Window )
	{
		return INVALID_ID;
	}
	p96RectFill(My_Window->RPort, 0,0, My_Screen->Width, My_Screen->Height, 0x00000000);

	mouse_hidden = TRUE;
	SetPointer(My_Window, EmptyPointer, 1, 16, 0, 0);

	if ( (ModeID = GetVPModeID(&My_Window->WScreen->ViewPort) ) == INVALID_ID)
   	{
		return INVALID_ID;
   	}

	SetWindowAttrs(My_Window,
                               WA_Left,left,
                               WA_Top,top,
                               WA_Width,out_width,
                               WA_Height,out_height,
                               TAG_DONE);

	vo_screenwidth = My_Screen->Width;
	vo_screenheight = My_Screen->Height;
	vo_dwidth = window_width;
	vo_dheight = window_height;
	vo_fs = 1;

	ScreenToFront(My_Screen);

	return ModeID;
}

static ULONG Open_FullScreen()
{ 

    ULONG ModeID = INVALID_ID;

    ModeID = GoFullScreen();
    return ModeID;
} 

/**************************/
static int PrepareVideo(uint32_t in_format, uint32_t out_format)
{
    g_in_format = in_format;
    g_out_format= out_format;

    return 0;
}


/******************************** CONFIG ******************************************/
static int config(uint32_t width, uint32_t height, uint32_t d_width,
             uint32_t d_height, uint32_t flags, char *title,
             uint32_t format)
{
    uint32_t out_format;
    ULONG ModeID = INVALID_ID;

	if (Stopped==TRUE || My_Window)
		FreeGfx();
		
	amiga_aspect_ratio = (float) d_width /  (float) d_height;

	if (d_width<MIN_WIDTH || d_height<MIN_HEIGHT)
	{
		d_width = MIN_WIDTH * amiga_aspect_ratio;
		d_height = MIN_HEIGHT * amiga_aspect_ratio;
	}
	//else
	//	d_height = d_width / amiga_aspect_ratio;

	if (old_d_width==0 || old_d_height==0)
	{
		old_d_width     = d_width;
		old_d_height    = d_height;
	}

	if (keep_width==0 || keep_height==0)
	{
		keep_width     = d_width;
		keep_height    = d_height;
	}

	amiga_image_width = width;
	amiga_image_height = height;
	amiga_image_format = format;

    is_fullscreen = flags & VOFLAG_FULLSCREEN;

	window_width  = d_width;
	window_height = d_height;

	EmptyPointer = AllocVec(12, MEMF_PUBLIC | MEMF_CLEAR);

	MutexObtain( window_mx );

	if ( is_fullscreen )
	{
		ModeID = Open_FullScreen();
		if (INVALID_ID == ModeID) ModeID = Open_PIPWindow();
	}
	else
		ModeID = Open_PIPWindow();

	if (My_Window)
	{
		rp = My_Window->RPort;
		UserMsg = My_Window->UserPort;
	}

	vo_draw_alpha_func = draw_alpha_yv12;
	out_format = IMGFMT_YV12;

	MutexRelease(window_mx);

	if (INVALID_ID == ModeID)
	{
		Printf("INVALID_ID == ModeID");
		return 1;
   	}

	if (PrepareVideo(format, out_format) < 0)
	{
		return 1;
	}

	Stopped = FALSE;
    return 0; // -> Ok
}

#define PLANAR_2_CHUNKY(Py, Py2, Pu, Pv, dst, dst2)	\
	{											\
	register ULONG Y = *++Py;				\
	register ULONG Y2= *++Py;				\
	register ULONG U = *++Pu;				\
	register ULONG V = *++Pv;				\
												\
	__asm__ __volatile__ (					\
		"rlwinm %%r3,%1,0,0,7;"			\
		"rlwinm %%r4,%1,16,0,7;"   		\
		"rlwimi %%r3,%2,24,8,15;"		\
		"rlwimi %%r4,%2,0,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,8,24,31;"		\
		"rlwimi %%r4,%3,16,24,31;"		\
												\
		"stw %%r3,4(%0);"			\
		"stwu %%r4,8(%0);"				\
												\
	    : "+b" (dst)						\
	    : "r" (Y), "r" (U), "r" (V) 		\
	    : "r3", "r4");				\
												\
	Y = *++Py2;							\
	__asm__ __volatile__ (				\
		"rlwinm %%r3,%1,0,0,7;"			\
		"rlwinm %%r4,%1,16,0,7;"			\
		"rlwimi %%r3,%2,8,8,15;"			\
		"rlwimi %%r4,%2,16,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,24,24,31;"		\
		"rlwimi %%r4,%3,0,24,31;"		\
												\
		"stw %%r3,4(%0);"		\
		"stwu %%r4,8(%0);"				\
											\
	    : "+b" (dst)						\
	    : "r" (Y2), "r" (U), "r" (V) 		\
	    : "r3", "r4");				\
												\
	Y2= *++Py2;							\
	__asm__ __volatile__ (				\
		"rlwinm %%r3,%1,0,0,7;" 		\
		"rlwinm %%r4,%1,16,0,7;"   		\
		"rlwimi %%r3,%2,24,8,15;"		\
		"rlwimi %%r4,%2,0,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,8,24,31;"		\
		"rlwimi %%r4,%3,16,24,31;"		\
												\
		"stw %%r3,4(%0);"			\
		"stwu %%r4,8(%0);"				\
												\
	    : "+b" (dst2)						\
	    : "r" (Y), "r" (U), "r" (V) 		\
	    : "r3", "r4");				\
									\
	__asm__ __volatile__ (				\
		"rlwinm %%r3,%1,0,0,7;"		\
		"rlwinm %%r4,%1,16,0,7;"		\
		"rlwimi %%r3,%2,8,8,15;"		\
		"rlwimi %%r4,%2,16,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,24,24,31;"		\
		"rlwimi %%r4,%3,0,24,31;"		\
												\
		"stw %%r3,4(%0);"				\
		"stwu %%r4,8(%0);"				\
												\
	    : "+b" (dst2)						\
	    : "r" (Y2), "r" (U), "r" (V) 		\
	    : "r3", "r4");					\
	}

#if USE_VMEM64
#define PLANAR_2_CHUNKY_64(Py, Py2, Pu, Pv, dst, dst2, tmp)	\
	{											\
	register ULONG Y = *++Py;				\
	register ULONG Y2= *++Py;				\
	register ULONG U = *++Pu;				\
	register ULONG V = *++Pv;				\
												\
	__asm__ __volatile__ (					\
		"rlwinm %%r3,%1,0,0,7;"			\
		"rlwinm %%r4,%1,16,0,7;"   		\
		"rlwimi %%r3,%2,24,8,15;"		\
		"rlwimi %%r4,%2,0,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,8,24,31;"		\
		"rlwimi %%r4,%3,16,24,31;"		\
												\
		"stw %%r3,0(%4);"			\
		"stw %%r4,4(%4);"			\
		"lfd %%f1,0(%4);"			\
		"stfdu %%f1,8(%0);"			\
												\
	    : "+b" (dst)						\
	    : "r" (Y), "r" (U), "r" (V), "r" (tmp) 		\
	    : "r3", "r4", "fr1");				\
												\
	Y = *++Py2;							\
	__asm__ __volatile__ (				\
		"rlwinm %%r3,%1,0,0,7;"			\
		"rlwinm %%r4,%1,16,0,7;"			\
		"rlwimi %%r3,%2,8,8,15;"			\
		"rlwimi %%r4,%2,16,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,24,24,31;"		\
		"rlwimi %%r4,%3,0,24,31;"		\
												\
		"stw %%r3,0(%4);"			\
		"stw %%r4,4(%4);"			\
		"lfd %%f1,0(%4);"			\
		"stfdu %%f1,8(%0);"			\
											\
	    : "+b" (dst)						\
	    : "r" (Y2), "r" (U), "r" (V), "r" (tmp)		\
	    : "r3", "r4", "fr1");				\
												\
	Y2= *++Py2;							\
	__asm__ __volatile__ (				\
		"rlwinm %%r3,%1,0,0,7;" 		\
		"rlwinm %%r4,%1,16,0,7;"   		\
		"rlwimi %%r3,%2,24,8,15;"		\
		"rlwimi %%r4,%2,0,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,8,24,31;"		\
		"rlwimi %%r4,%3,16,24,31;"		\
												\
		"stw %%r3,0(%4);"			\
		"stw %%r4,4(%4);"			\
		"lfd %%f1,0(%4);"			\
		"stfdu %%f1,8(%0);"			\
												\
	    : "+b" (dst2)						\
	    : "r" (Y), "r" (U), "r" (V), "r" (tmp) 		\
	    : "r3", "r4", "fr1");				\
									\
	__asm__ __volatile__ (				\
		"rlwinm %%r3,%1,0,0,7;"		\
		"rlwinm %%r4,%1,16,0,7;"		\
		"rlwimi %%r3,%2,8,8,15;"		\
		"rlwimi %%r4,%2,16,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,24,24,31;"		\
		"rlwimi %%r4,%3,0,24,31;"		\
												\
		"stw %%r3,0(%4);"			\
		"stw %%r4,4(%4);"			\
		"lfd %%f1,0(%4);"			\
		"stfdu %%f1,8(%0);"			\
												\
	    : "+b" (dst2)						\
	    : "r" (Y2), "r" (U), "r" (V), "r" (tmp) 		\
	    : "r3", "r4", "fr1");					\
	}
#endif

/******************************** DRAW_SLICE ******************************************/

static int draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	struct RenderInfo ri;
	struct BitMap *bm = NULL;
	int lock_h;
	unsigned int w3, _w2;

	MutexObtain( window_mx );
	if (!My_Window) 
	{
		MutexRelease(window_mx);
		return 0;
	}

/*
    w -= (w%8);
*/
    p96PIP_GetTags(My_Window, P96PIP_SourceBitMap   , (ULONG)&bm, TAG_DONE );

    if (! (lock_h = p96LockBitMap(bm, (uint8 *)&ri, sizeof(struct RenderInfo))))
    {
        // Unable to lock the BitMap -> do nothing
        return -1;
    }

    w3 = w & -8;
    _w2 = w3 / 8; // Because we write 8 pixels (aka 16 bit word) at each loop
    h &= -2;

    if (h > 0 && _w2)
    {
        ULONG *srcdata, *srcdata2;

        ULONG *py, *py2, *pu, *pv;
        ULONG *_py, *_py2, *_pu, *_pv;

        _py = py = (ULONG *) image[0];
        _pu = pu = (ULONG *) image[1];
        _pv = pv = (ULONG *) image[2];

        srcdata = (ULONG *) ((uint32)ri.Memory + ( ( (y * amiga_image_width) + x) * 2));

        // The data must be stored like this:
        // YYYYYYYY UUUUUUUU : pixel 0
        // YYYYYYYY VVVVVVVV : pixel 1

        // We will read one word of Y (4 byte)
        // then read one word of U and V
        // After another Y word because we need 2 Y for 1 (U & V)

        _py--;
        _pu--;
        _pv--;

        stride[0] &= -8;
        stride[1] &= -8;
        stride[2] &= -8;

#if USE_VMEM64
        /* If aligned buffer, use 64bit writes. It might be worth
           the effort to manually align in other cases, but that'd
           need to handle all conditions such like:
           a) UWORD, UQUAD ... [end aligment]
           b) ULONG, UQUAD ... [end aligment]
           c) ULONG, UWORD, UQUAD ... [end aligment]
           - Piru
        */
        if (!(((uint32) srcdata) & 7))
        {
            ULONG _tmp[3];
            // this alignment crap is needed in case we don't have aligned stack
            register ULONG *tmp = (APTR)((((ULONG) _tmp) + 4) & -8);

            srcdata -= 2;

            do
            {
                int w2 = _w2;
                h -= 2;

                _py2 = (ULONG *)(((ULONG)_py) + (stride[0] ) );

                py = _py;
                py2 = _py2;
                pu = _pu;
                pv = _pv;

                srcdata2 = (ULONG *) ( (ULONG) srcdata + w3 * 2 );

                do
                {
                    // Y is like that : Y1 Y2 Y3 Y4
                    // U is like that : U1 U2 U3 U4
                    // V is like that : V1 V2 V3 V4

                    PLANAR_2_CHUNKY_64(py, py2, pu, pv, srcdata, srcdata2, tmp);

                } while (--w2);

                srcdata = (ULONG *) ( (ULONG) srcdata + (w3 << 1) );
                _py = (ULONG *)(((ULONG)_py) + (stride[0] << 1) );
                _pu = (ULONG *)(((ULONG)_pu) + stride[1]);
                _pv = (ULONG *)(((ULONG)_pv) + stride[2]);

            } while (h);
        }
        else
#endif
        {
            srcdata--;

            do
            {
                int w2 = _w2;

                h -= 2;

                _py2 = (ULONG *)(((ULONG)_py) + (stride[0] ) );

                py = _py;
                py2 = _py2;
                pu = _pu;
                pv = _pv;

                srcdata2 = (ULONG *) ( (ULONG) srcdata + w3 * 2 );

                do
                {
                    // Y is like that : Y1 Y2 Y3 Y4
                    // U is like that : U1 U2 U3 U4
                    // V is like that : V1 V2 V3 V4

                    PLANAR_2_CHUNKY(py, py2, pu, pv, srcdata, srcdata2);

                } while (--w2);

                srcdata = (ULONG *) ( (ULONG) srcdata + (w3 << 1) );
                _py = (ULONG *)(((ULONG)_py) + (stride[0] << 1) );
                _pu = (ULONG *)(((ULONG)_pu) + stride[1]);
                _pv = (ULONG *)(((ULONG)_pv) + stride[2]);

            } while (h);
        }
    }


	p96UnlockBitMap(bm, lock_h);

	MutexRelease(window_mx);

	return 0;
}
/******************************** DRAW_OSD ******************************************/

static void draw_osd(void)
{
    vo_draw_text(amiga_image_width, amiga_image_height, vo_draw_alpha_func);
}
/******************************** FLIP_PAGE ******************************************/
static void flip_page(void)
{

	struct RenderInfo ri;
	struct BitMap *bm;
	ULONG lock_h;
	uint32 WorkBuf, DispBuf, NextWorkBuf;

	if (numbuffers>1)
	{
//		p96PIP_GetTags(My_Window, P96PIP_SourceBitMap   , (ULONG)&bm ,  TAG_DONE );

//		if ( (lock_h = p96LockBitMap(bm, (uint8 *)&ri, sizeof(struct RenderInfo))))
//	   	{
			p96PIP_GetTags(My_Window,	P96PIP_WorkBuffer, &WorkBuf,
								P96PIP_DisplayedBuffer, &DispBuf,
								TAG_END, TAG_DONE );

			NextWorkBuf = (WorkBuf+1) % numbuffers;

			if ((NextWorkBuf == DispBuf)&&(gfx_novsync==FALSE)) WaitTOF();

			p96PIP_SetTags(My_Window,	P96PIP_VisibleBuffer, WorkBuf,
						 		P96PIP_WorkBuffer,    NextWorkBuf,
						 		TAG_END, TAG_DONE );

//			p96UnlockBitMap(bm, lock_h);
//		}
	}

   /* nothing */
}
/******************************** DRAW_FRAME ******************************************/
static int draw_frame(uint8_t *src[])
{
	// Nothing
	return -1;
}

/************************************************************************************/
/* Just a litle func to help uninit and control										*/
/* it close screen (if fullscreen), the windows, and free all gfx related stuff		*/
/* but do not close any libs														*/
/************************************************************************************/

static void FreeGfx(void)
{
	if (EmptyPointer)
	{
		if (My_Window) ClearPointer(My_Window);
		FreeVec(EmptyPointer);
		EmptyPointer = NULL;
	}
	
	MutexObtain( window_mx );

	if (My_Window)
	{
		p96PIP_Close(My_Window);
		My_Window = NULL;
	}

	if (My_Screen)
   	{
		CloseScreen(My_Screen);
		My_Screen = NULL;
   	}

	MutexRelease(window_mx);
}
/******************************** UNINIT    ******************************************/
static void uninit(void)
{
	    FreeGfx();
}

/******************************** CONTROL ******************************************/
static int control(uint32_t request, void *data, ...)
{
    switch (request)
    {
        case VOCTRL_GUISUPPORT:
            return VO_FALSE;
		break;
        case VOCTRL_FULLSCREEN:
			is_fullscreen ^= VOFLAG_FULLSCREEN;

			FreeGfx(); // Free/Close all gfx stuff (screen windows, buffer...);
			if ( config(amiga_image_width, amiga_image_height, window_width, window_height, is_fullscreen, NULL, amiga_image_format) < 0) return VO_FALSE;

            return VO_TRUE;
		break;
        case VOCTRL_ONTOP:
        	vo_ontop = !vo_ontop;
        	is_ontop = !is_ontop;
        	
			FreeGfx(); // Free/Close all gfx stuff (screen windows, buffer...);
			if ( config(amiga_image_width, amiga_image_height, window_width, window_height, FALSE, NULL, amiga_image_format) < 0) return VO_FALSE;
            return VO_TRUE;
		break;
		case VOCTRL_UPDATE_SCREENINFO:
			if (is_fullscreen)
			{
				vo_screenwidth = My_Screen->Width;
				vo_screenheight = My_Screen->Height;
			}
			else
			{
				struct Screen *scr = LockPubScreen (NULL);
				if(scr)
				{
					vo_screenwidth = scr->Width;
					vo_screenheight = scr->Height;
					UnlockPubScreen(NULL, scr);
				}
	        }
	        aspect_save_screenres(vo_screenwidth, vo_screenheight);
			return VO_TRUE;
		break;
        case VOCTRL_PAUSE:
            return VO_TRUE;
		break;
        case VOCTRL_RESUME:
            return VO_TRUE;
		break;
        case VOCTRL_QUERY_FORMAT:
            return query_format(*(ULONG *)data);
		break;
    }

    return VO_NOTIMPL;
}

/******************************** CHECK_EVENTS    ******************************************/

static void check_events(void)
{
	gfx_CheckEvents(My_Screen, My_Window, &window_height, &window_width, &win_left, &win_top);
}



static int query_format(uint32_t format)
{
    switch( format)
    {
	    case IMGFMT_YV12:
	    case IMGFMT_YUY2:
	    case IMGFMT_I420:
    	case IMGFMT_UYVY:
	    case IMGFMT_YVYU:
    	    return  VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_OSD |
        	    	VFCAP_HWSCALE_UP 	| VFCAP_HWSCALE_DOWN 		| VFCAP_ACCEPT_STRIDE;

	    case IMGFMT_RGB15:
    	case IMGFMT_BGR15:
	    case IMGFMT_RGB16:
    	case IMGFMT_BGR16:
	    case IMGFMT_RGB24:
	    case IMGFMT_BGR24:
	    case IMGFMT_RGB32:
	    case IMGFMT_BGR32:
    	    return VFCAP_CSP_SUPPORTED | VFCAP_OSD | VFCAP_FLIP;

        default:
            return VO_FALSE;
    }
}


#if 0
static void ClearSpaceArea(struct Window *window)
{
	struct IBox* ibox;
	IIntuition->GetAttr(SPACE_AreaBox, Space_Object, (uint32*)(void*)&ibox);
	IGraphics->SetAPen(window->RPort,1);
	IGraphics->RectFill(window->RPort,ibox->Left,ibox->Top,ibox->Left+ibox->Width-1,ibox->Top+ibox->Height-1);
	IIntuition->ChangeWindowBox(window, window->LeftEdge, window->TopEdge, window->Width, window->Height);
}
#endif



