/*
 *  vo_gfx_vmem.c
 *  VO module for MPlayer MorphOS & AmigaOS4
 *  using CGX/direct VMEM
 *  Writen by DET Nicolas
 *  Maintained and updated by Fabien Coeurjoly
*/

#define SYSTEM_PRIVATE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "config.h"

#include "mp_msg.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "cgx_common.h"
//#include "version.h"
#include "sub/osd.h"
#include "sub/sub.h"

#include "../ffmpeg/libswscale/swscale.h"
#include "../ffmpeg/libswscale/swscale_internal.h" //FIXME
#include "../ffmpeg/libswscale/rgb2rgb.h"
#include "../libmpcodecs/vf_scale.h"

#ifdef __morphos__
#include "../morphos_stuff.h"
#endif

#ifdef CONFIG_GUI
#include "gui/interface.h"
#include "gui/morphos/gui.h"
#include "mplayer.h"
#endif

#include <inttypes.h>		// Fix <postproc/rgb2rgb.h> bug

//Debug
#define kk(x)
#define dprintf(...)

#include <proto/Picasso96API.h>

#include <graphics/gfxbase.h>

#include <proto/intuition.h>
//#include <intuition/extensions.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <exec/types.h>
#include <dos/dostags.h>
#include <dos/dos.h>

#include <graphics/gfxbase.h>

extern APTR window_mx;

static vo_info_t info =
{
#ifdef __amigaos4__
	"Graphics.library / WritePixelArray (no window scaling)",
	"wpa",
#endif
#if defined (__morphos__) || defined(__amigaos3__)
	"CyberGraphX video output (WPA)",
	"cgx_wpa",
#endif
	"DET Nicolas",
	"AmigaOS4 rules da world !"
};

LIBVO_EXTERN(cgx_wpa)

extern struct Library *GraphicsBase;

static BOOL FirstTime = TRUE;

static struct Window *         My_Window      = NULL;
static struct Screen *         My_Screen      = NULL;
static struct Window *         bf_Window      = NULL;

static struct RastPort *       rp             = NULL;

static struct MsgPort *        UserMsg        = NULL;
static UBYTE *                 image_buffer_mem = NULL;
static UBYTE *                 image_buffer     = NULL;

// not OS specific
static uint32_t   image_width;            // well no comment
static uint32_t   image_height;
static uint32_t   window_width;           // width and height on the window
static uint32_t   window_height;          // can be different from the image
static uint32_t   screen_width;           // Indicates the size of the screen in full screen
static uint32_t   screen_height;          // Only use for triple buffering

extern float amiga_aspect_ratio;

extern uint32_t	is_fullscreen;

extern UWORD *EmptyPointer;               // Blank pointer
extern ULONG WantedModeID;

static uint32_t   image_bpp;            	// image bpp
static uint32_t   offset_x;               	// offset in the rp where we have to display the image
static uint32_t   offset_y;               	// ...
static uint32_t   internal_offset_x;        // Indicate where to render the picture inside the bitmap
static uint32_t   internal_offset_y;        // ...

static uint32_t		image_format;

static float best_screen_aspect_ratio;

static uint32_t   win_left;             
static uint32_t   win_top;              

static SwsContext *swsContext=NULL;

static vo_draw_alpha_func draw_alpha_func;

/***************************************/
struct BackFillArgs
{
	struct Layer     *layer;
	struct Rectangle  bounds;
	LONG              offsetx;
	LONG              offsety;
};

#ifdef __amigaos4__
static void BackFill_Func(struct RastPort *ArgRP, struct BackFillArgs *MyArgs);
static struct Hook BackFill_Hook =
{
	{NULL, NULL},
	(HOOKFUNC) &BackFill_Func,
	NULL,
	NULL
};
#else
static void BackFill_Func(void);
static const struct EmulLibEntry BackFill_Func_gate =
{
	TRAP_LIB,
	0,
	(void (*)(void)) BackFill_Func
};

static struct Hook BackFill_Hook =
{
	{NULL, NULL},
	(HOOKFUNC) &BackFill_Func_gate,
	NULL,
	NULL
};
#endif

#ifdef __GNUC__
# pragma pack(2)
#endif

#ifdef __GNUC__
# pragma pack()
#endif

#ifdef __amigaos4__
static void BackFill_Func(struct RastPort *ArgRP, struct BackFillArgs *MyArgs)
#else
static void BackFill_Func(void)
#endif
{

	PREPARE_BACKFILL(window_width, window_height);

	WritePixelArray(
		image_buffer,	
		BufferStartX,
		BufferStartY,
		window_width*image_bpp,
		PIXF_A8R8G8B8,
		&MyRP,
		StartX,
		StartY,
		SizeX,
		SizeY);
}


/******************************** DRAW ALPHA ******************************************/
// Draw_alpha series !
static void draw_alpha_null (int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
	// Nothing
}

static void draw_alpha_rgb32 (int x0,int y0, int w,int h, unsigned char* src, unsigned char * srca, int stride)
{
	vo_draw_alpha_rgb32(w,h,src,srca,
				stride,
				(UBYTE *) ( (ULONG) image_buffer + (y0*window_width+x0)*image_bpp), window_width*image_bpp);
}

/******************************** PREINIT ******************************************/
static int preinit(const char *arg)
{
	mp_msg(MSGT_VO, MSGL_INFO, "VO: [wpa] Welcome man !.\n");

	if ( ! LIB_IS_AT_LEAST(GraphicsBase, 54,100) )
	{
		mp_msg(MSGT_VO, MSGL_INFO, "VO: [wpa] You need graphics.library version 54.100 or newer.\n");	
		return -1;
	}

	if (!gfx_GiveArg(arg))
	{
		return -1;	
	}

	gfx_Message();

	if ( ( My_Screen = LockPubScreen ( gfx_screenname ) ) )
	{
		best_screen_aspect_ratio = (float) My_Screen -> Width / (float) My_Screen -> Height;
		UnlockPubScreen(NULL, My_Screen);
	}

	return 0;
}

extern char * filename;

static ULONG Open_Window()
{		
	// Window
	ULONG ModeID = INVALID_ID;
	BOOL WindowActivate = TRUE;

	My_Window = NULL;

	dprintf("%s:%ld\n",__FUNCTION__,__LINE__);

	if ( ( My_Screen = LockPubScreen ( gfx_screenname ) ) )
	{
		ModeID = GetVPModeID(&My_Screen->ViewPort);

		if (FirstTime)
		{
			gfx_center_window(My_Screen, window_width, window_height, &win_left, &win_top);
			FirstTime = FALSE;
		}

		switch(gfx_BorderMode)
		{
			case NOBORDER:
				My_Window = OpenWindowTags( NULL,
					WA_CustomScreen,    (ULONG) My_Screen,
					WA_Left,            win_left,
					WA_Top,             win_top,
					WA_InnerWidth,      window_width,
					WA_InnerHeight,     window_height,
					WA_SimpleRefresh,   TRUE,
					WA_CloseGadget,     FALSE,
					WA_DepthGadget,     FALSE,
					WA_DragBar,         FALSE,
					WA_Borderless,      TRUE,
					WA_SizeGadget,      FALSE,
					WA_NewLookMenus,	TRUE,
					WA_Activate,        WindowActivate,
					WA_IDCMP,           IDCMP_COMMON,
					WA_Flags,           WFLG_REPORTMOUSE,
					//WA_SkinInfo,				NULL,
				TAG_DONE);
				break;

			case TINYBORDER:
				My_Window = OpenWindowTags( NULL,
					WA_CustomScreen,    (ULONG) My_Screen,
					WA_Left,            win_left,
					WA_Top,             win_top,
					WA_InnerWidth,      window_width,
					WA_InnerHeight,     window_height,
					WA_SimpleRefresh,   TRUE,
					WA_CloseGadget,     FALSE,
					WA_DepthGadget,     FALSE,
					WA_DragBar,         FALSE,
					WA_Borderless,      FALSE,
					WA_SizeGadget,      FALSE,
					WA_NewLookMenus,	TRUE,
					WA_Activate,        WindowActivate,
					WA_IDCMP,           IDCMP_COMMON,
					WA_Flags,           WFLG_REPORTMOUSE,
					//WA_SkinInfo,				NULL,
				TAG_DONE);	
				break;

			default:
				My_Window = OpenWindowTags( NULL,
					WA_CustomScreen,    (ULONG) My_Screen,
					WA_ScreenTitle,     (ULONG) gfx_common_screenname,
#ifdef __morphos__
					WA_Title,         (ULONG) filename ? MorphOS_GetWindowTitle() : "MPlayer for MorphOS",
#endif
#ifdef __amigaos4__
					WA_Title,         "MPlayer for AmigaOS4",
#endif
					WA_Left,            win_left,
					WA_Top,             win_top,
					WA_InnerWidth,      window_width,
					WA_InnerHeight,     window_height,
					WA_SimpleRefresh,   TRUE,
					WA_CloseGadget,     TRUE,
					WA_DepthGadget,     TRUE,
					WA_DragBar,         TRUE,
					WA_Borderless,      (gfx_BorderMode == NOBORDER) ? TRUE : FALSE,
					WA_SizeGadget,      FALSE,
					WA_SizeBBottom,	TRUE,
					WA_NewLookMenus,	TRUE,
					WA_Activate,        WindowActivate,
					WA_IDCMP,           IDCMP_COMMON,
					WA_Flags,           WFLG_REPORTMOUSE,
					//WA_SkinInfo,        NULL,
					TAG_DONE);
		}

		vo_screenwidth = My_Screen->Width;
		vo_screenheight = My_Screen->Height;
		vo_dwidth = My_Window->Width;
		vo_dheight = My_Window->Height;

		vo_fs = 0;

		UnlockPubScreen(NULL, My_Screen);
		My_Screen= NULL;
	}

		EmptyPointer = AllocVec(16, MEMF_PUBLIC | MEMF_CLEAR);

	if ( !My_Window || !EmptyPointer)
	{
		mp_msg(MSGT_VO, MSGL_ERR, "Unable to open a window\n");

		if (gfx_screenname) 
		{
			// open on workbench if no pubscreen found.
			free(gfx_screenname);
			gfx_screenname = NULL;
		}

		uninit();
		return INVALID_ID;
	}

	offset_x = (gfx_BorderMode == NOBORDER) ? 0 : My_Window->BorderRight;	
	offset_y = (gfx_BorderMode == NOBORDER) ? 0 : My_Window->BorderTop;

	internal_offset_x = 0;
	internal_offset_y = 0;

	if ( (ModeID = GetVPModeID(&My_Window->WScreen->ViewPort) ) == INVALID_ID) 
	{
		uninit();
		return INVALID_ID;
	}

	ScreenToFront(My_Window->WScreen);

	gfx_StartWindow(My_Window);
	gfx_ControlBlanker(My_Screen, FALSE);

	return ModeID;
}

static ULONG Open_FullScreen()
{ 
	// if fullscreen -> let's open our own screen
	// It is not a very clean way to get a good ModeID, but a least it works
	ULONG depth;
	ULONG ModeID;

	if(WantedModeID)
	{
		ModeID = WantedModeID;
	}
	else
	{
		ModeID = find_best_screenmode( gfx_monitor, 32, best_screen_aspect_ratio,image_width, image_height);
	}

	if ( ModeID == INVALID_ID) 
	{
		uninit();
		return INVALID_ID;
	}

	mp_msg(MSGT_VO, MSGL_INFO, "VO: [wpa] Full screen.\n");

	if ( ModeID != INVALID_ID )
	{

#ifdef __morphos__
		depth = ( FALSE ) ? 16 : GetCyberIDAttr( CYBRIDATTR_DEPTH , ModeID);
#endif
#ifdef __amigaos4__
		depth = ( FALSE ) ? 16 : p96GetModeIDAttr( ModeID, P96IDA_DEPTH );
#endif

		My_Screen = OpenScreenTags ( NULL,
			SA_DisplayID,  ModeID,
#ifdef PUBLIC_SCREEN
				SA_Type, PUBLICSCREEN,
				SA_PubName, "MPlayer Screen",
#else
				SA_Type, CUSTOMSCREEN,
#endif
			SA_Title, "MPlayer Screen",
			SA_ShowTitle, FALSE,
			SA_Quiet, 	TRUE,
			SA_LikeWorkbench, TRUE,
		TAG_DONE);
	}

	 if ( ! My_Screen ) 
	{
		mp_msg(MSGT_VO, MSGL_ERR, "Unable to open the screen ID:%x\n", (int) ModeID);
	  uninit();
	  return INVALID_ID;
	}

#if PUBLIC_SCREEN
	PubScreenStatus( My_Screen, 0 );
#endif

	screen_width = My_Screen->Width;
	screen_height = My_Screen->Height;

	offset_x = (screen_width - window_width)/2;
	offset_y = (screen_height - window_height)/2;
	internal_offset_x = 0;
	internal_offset_y = 0;

	bf_Window = OpenWindowTags( NULL,
			WA_PubScreen,       (ULONG) My_Screen,
			WA_Top,		0,
			WA_Left,		0,
			WA_Height,		vo_screenheight,
			WA_Width,		vo_screenwidth,
			WA_SimpleRefresh,   TRUE,
			WA_CloseGadget,     FALSE,
			WA_DragBar,         FALSE,
			WA_Borderless,      TRUE,
			WA_Backdrop,        TRUE,
			WA_Activate,        FALSE,
		TAG_DONE);

	if (bf_Window)
	{
		/* Fill the screen with black color */
		RectFillColor(bf_Window->RPort, 0,0, My_Screen->Width, My_Screen->Height, 0x00000000);
	}

	My_Window = OpenWindowTags( NULL,
#if PUBLIC_SCREEN
			WA_PubScreen,  (ULONG) My_Screen,
#else
			WA_CustomScreen,  (ULONG) My_Screen,
#endif
			WA_PubScreen,       (ULONG) My_Screen,
			WA_Top,             0,
			WA_Left,            0,
			WA_Height,          screen_height,
			WA_Width,           screen_width,
			WA_SimpleRefresh,   TRUE,
			WA_CloseGadget,     FALSE,
			WA_DragBar,         TRUE,
			WA_Borderless,      TRUE,
			WA_Backdrop,        TRUE,
			WA_NewLookMenus,	TRUE,
			WA_Activate,        TRUE,
			WA_IDCMP,           IDCMP_COMMON,
			WA_Flags,           WFLG_REPORTMOUSE,
		TAG_DONE);

	if ( ! My_Window) 
	{
		mp_msg(MSGT_VO, MSGL_ERR, "Unable to open a window\n");
		uninit();
		return INVALID_ID;
	}

	RectFillColor( My_Window->RPort, 0,0, screen_width, screen_height, 0x00000000);
//	FillPixelArray( My_Window->RPort, 0,0, screen_width, screen_height, 0x00000000);
/*
	vo_screenwidth = My_Screen->Width;
	vo_screenheight = My_Screen->Height;
	vo_dwidth = vo_screenwidth - 2*offset_x;
	vo_dheight = vo_screenheight - 2*offset_y;
*/
	vo_fs = 1;

	gfx_ControlBlanker(My_Screen, FALSE);
	return ModeID;
} 

static int PrepareBuffer(uint32_t in_format, uint32_t out_format)
{
#define ALIGN 64
	if ( ! (image_buffer_mem = (UBYTE *) AllocVec ( image_bpp * window_width * window_height + ALIGN - 1, MEMF_ANY ) ) ) {
		uninit();
		return -1;
	}

	image_buffer = (APTR) ((((ULONG) image_buffer_mem) + ALIGN - 1) & ~(ALIGN - 1));
#undef ALIGN

#if 1
	swsContext= sws_getContextFromCmdLine(image_width, image_height, in_format, window_width, window_height, out_format );
  if (!swsContext)
	{
		uninit();
	return -1;
	}

#else
  yuv2rgb_init( image_depth, pixel_format ); // if the pixel format is unknow, mplayer will select just a bad one !
#endif


	return 0;
}


/******************************** CONFIG ******************************************/
static int config(uint32_t width, uint32_t height, uint32_t d_width,
		     uint32_t d_height, uint32_t flags, char *title,
		     uint32_t in_format)
{
	ULONG ModeID = INVALID_ID;

	if (My_Window) return 0;

	is_fullscreen = flags & VOFLAG_FULLSCREEN;
	image_format = in_format;

	// backup info
	image_bpp = 4;
	image_width = width & -8;
	image_height = height & -2;
	window_width = d_width & -8;
	window_height = d_height & -2;

	amiga_aspect_ratio = (float) d_width /  (float) d_height;

	ModeID = INVALID_ID;

	if ( is_fullscreen )
	{
		dprintf("%s:%ld\n",__FUNCTION__,__LINE__);
 		ModeID = Open_FullScreen();
	}

	if (ModeID == INVALID_ID)
	{
		is_fullscreen &= ~VOFLAG_FULLSCREEN;	// we do not have fullscreen
		ModeID = Open_Window();
	}

	if (My_Window)
	{
		attach_menu(My_Window);
	}

	if (!My_Window)
	{
		uninit();
		return -1;
	}

	rp = My_Window->RPort;
	UserMsg = My_Window->UserPort;


dprintf("%s:%ld\n",__FUNCTION__,__LINE__);

 	if (PrepareBuffer(in_format, IMGFMT_BGR32) < 0) 
	{
		uninit();
		return -1;
	}

dprintf("%s:%ld\n",__FUNCTION__,__LINE__);

	draw_alpha_func = draw_alpha_rgb32;

dprintf("%s:%ld\n",__FUNCTION__,__LINE__);

	gfx_Start(My_Window);

dprintf("%s:%ld\n",__FUNCTION__,__LINE__);

#ifdef CONFIG_GUI
    if (use_gui)
		guiGetEvent(guiSetWindowPtr, (char *) My_Window);
#endif

dprintf("%s:%ld\n",__FUNCTION__,__LINE__);

	return 0; // -> Ok
}

/******************************** DRAW_SLICE ******************************************/
static int draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
#if 1
	uint8_t *dst[3];
  int dstStride[3];

	dstStride[0] = window_width*image_bpp;
	dstStride[1] = 0;
	dstStride[2] = 0;

	dst[0] = (uint8_t *) ( (ULONG) image_buffer +x*image_bpp) ;
	dst[1] = NULL;
	dst[2] = NULL; 

	sws_scale(swsContext, 
			image,
			stride,
			y + internal_offset_y,
			h,
			dst, 
			dstStride);
#else
	w -= (w%2);
	yuv2rgb( (UBYTE *) ( (ULONG) image_buffer + (y*image_width+x)*image_bpp) , image[0], image[1], image[2],\
									 w, h, image_width*image_bpp,
									 image_width, image_width/2 );
#endif
	return 0;
}
/******************************** DRAW_OSD ******************************************/

static void draw_osd(void)
{
	vo_draw_text(window_width,window_height,draw_alpha_func);
}

/******************************** FLIP_PAGE ******************************************/
static void flip_page(void)
{

	WritePixelArray(
		image_buffer,	
		0,
		0,
		window_width*image_bpp,
		PIXF_A8R8G8B8,
		rp,		
		offset_x,
		offset_y,
		window_width,
		window_height);

#warning "use rectfmt_raw if screen pixfmt is suited"
}
/******************************** DRAW_FRAME ******************************************/
static int draw_frame(uint8_t *src[])
{
	// Nothing
	return -1;
}


/***********************/
// Just a litle func to help uninit and control
// it close screen (if fullscreen), the windows, and free all gfx related stuff
// but do not clsoe any libs

static void FreeGfx(void)
{

#ifdef CONFIG_GUI
	if (use_gui)
	{
		guiGetEvent(guiSetWindowPtr, NULL);
		mygui->screen = My_Screen;
	}
#endif

	gfx_ControlBlanker(My_Screen, TRUE);
	gfx_Stop(My_Window);

	MutexObtain( window_mx );

	if (bf_Window) 
	{
		CloseWindow(bf_Window);
		bf_Window=NULL;
	}

	if (My_Window) 
	{
		gfx_StopWindow(My_Window);
		detach_menu(My_Window);
		CloseWindow(My_Window);
		My_Window=NULL;
	}

	if (My_Screen) 
	{
		CloseScreen(My_Screen);
		My_Screen = NULL;
	}

	rp = NULL;

	MutexRelease(window_mx);

	if (image_buffer_mem) {
		FreeVec(image_buffer_mem);
		image_buffer_mem = NULL;
	}
}

/******************************** UNINIT    ******************************************/
static void uninit(void)
{
dprintf("%s:%ld - rp is 0x%08lx \n",__FUNCTION__,__LINE__,rp);

	FreeGfx();

	if (EmptyPointer)
	{
	  FreeVec(EmptyPointer);
	  EmptyPointer=NULL;
	}

	gfx_ReleaseArg();
}

/******************************** CONTROL ******************************************/
static int control(uint32_t request, void *data)
{
	switch (request) 
	{
		case VOCTRL_GUISUPPORT:
			return VO_TRUE;

		case VOCTRL_FULLSCREEN:
			is_fullscreen ^= VOFLAG_FULLSCREEN;
			FreeGfx();
			if ( config(image_width, image_height, window_width, window_height, is_fullscreen, NULL, image_format) < 0) return VO_FALSE;
			return VO_TRUE;

		case VOCTRL_PAUSE:
			gfx_Stop(My_Window);
			if (rp) InstallLayerHook(rp->Layer, &BackFill_Hook);
			gfx_ControlBlanker(My_Screen, TRUE);
			return VO_TRUE;					

		case VOCTRL_RESUME:
			gfx_Start(My_Window);
			if (rp) InstallLayerHook(rp->Layer, NULL);
			gfx_ControlBlanker(My_Screen, FALSE);
			return VO_TRUE;

		case VOCTRL_QUERY_FORMAT:
			return query_format(*(ULONG *)data);
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
		case IMGFMT_I420:
		case IMGFMT_IYUV:
			return VO_TRUE;
		default:
			return VO_FALSE;
	}
}
