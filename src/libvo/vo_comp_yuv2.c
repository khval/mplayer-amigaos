/*
 *  vo_comp.c
 *  VO module for MPlayer AmigaOS4
 *  using Composition / direct VMEM
 *  Writen by Kjetil Hvalstrand
 *  Some code comes from Hans De Ruiter
 *  based on work by DET Nicolas and FAB, see vo_wpa_cgx.c
 */

#define SYSTEM_PRIVATE

#define NUM_BUFFERS 3

#define __copy_to_vram__

#undef __USE_INLINE__

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

#include "aspect.h"

#if HAVE_ALTIVEC
#ifdef memcpy
#undef memcpy
#endif 
#define memcpy(des,src,size) ALTIVEC_memcpy(des,src,size)
#else
#define memcpy(des,src,size) IExec->CopyMemQuick(src,dst,size)
#endif 

//--becnhmark--
static struct timezone dontcare = { 0,0 };
static struct timeval before, after;
static ULONG benchmark_frame_cnt = 0;
extern int benchmark;
//------------------

#include "../ffmpeg/libswscale/swscale.h"
#include "../ffmpeg/libswscale/swscale_internal.h" //FIXME
#include "../ffmpeg/libswscale/rgb2rgb.h"
#include "../libmpcodecs/vf_scale.h"

#ifdef CONFIG_GUI
#include "gui/interface.h"
#include "gui/morphos/gui.h"
#include "mplayer.h"
#endif

#include <inttypes.h>		// Fix <postproc/rgb2rgb.h> bug

#define debug_level 0

#if debug_level > 0
#define dprintf( ... ) IDOS->Printf( __VA_ARGS__ )
#else
#define dprintf(...) 
#endif


static int32 vo_format = 0;

#include <graphics/gfxbase.h>

#include <proto/intuition.h>
#include <proto/graphics.h>
#include <graphics/composite.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <exec/types.h>
#include <dos/dostags.h>
#include <dos/dos.h>

//struct Task *init_task = NULL;
extern APTR window_mx;

static BOOL FirstTime = TRUE;

// --------- for vo process ----------

static struct Process *MainTask = NULL;
static struct Process *vo_process = NULL;
static ULONG sig_vo_osd = 0;
static ULONG sig_vo_pageflip = 0;
static ULONG sig_vo_openwindow = 0;
static ULONG sig_vo_closewindow = 0;
static ULONG sig_vo_events = 0;
static ULONG ModeID;
static ULONG arg_d_width,arg_d_height;
// --------------------------------------

static void StartVOProcess( void );
static void StopVOProcess( void );

extern void attach_menu(struct Window *window);
extern void detach_menu(struct Window *window);

void draw_comp(struct BitMap *the_bitmap,struct Window * the_win, int width,int height);

static vo_info_t info =
{
	"Composition, yuv420p, direct rendering, has a thread for video output",
	"comp_yuv2",
	"Kjetil Hvalstrand",
	"AmigaOS4 rules da world !"
};


LIBVO_EXTERN(comp_yuv2)

struct XYSTW_Vertex3D { 
float x, y; 
float s, t, w; 
}; 

// ----- From vo_XV ------
static int buffer_allocated = 0;
static int current_buf = 0;
static int current_ip_buf = 0;
static int num_buffers = 1;     // default
static int visible_buf = 0;    
//-----------------------------

static struct BitMap *ram_bitmap[NUM_BUFFERS];
static struct BitMap *vram_bitmap = NULL;

static uint32_t   org_amiga_image_width;	// well no comment
static uint32_t   org_amiga_image_height;

extern uint32_t	amiga_image_width;	// well no comment
extern uint32_t	amiga_image_height;
extern float		amiga_aspect_ratio;

static uint32_t get_image(mp_image_t * mpi);
static uint32_t draw_image(mp_image_t * mpi);

static float best_screen_aspect_ratio;

typedef struct CompositeHookData_s {
	struct BitMap *srcBitMap; // The source bitmap
	int32 srcWidth, srcHeight; // The source dimensions
	int32 offsetX, offsetY; // The offsets to the destination area relative to the window's origin
	int32 scaleX, scaleY; // The scale factors
	uint32 retCode; // The return code from CompositeTags()
} CompositeHookData;

static CompositeHookData hookData;
static struct Rectangle rect;
static struct Hook hook;

static struct Window *         My_Window      = NULL;
static struct Screen *         the_screen      = NULL;

static vo_draw_alpha_func draw_alpha_func;

void clear_plain_half( int h, char *memory, ULONG value, uint32 BytesPerRow);

static void  alloc_output_buffer( uint32 width,uint32 height, uint32 format )
{
	int n;
	APTR lock;
	struct PlanarYUVInfo yuvInfo;

	amiga_image_width = width ;
	amiga_image_height = height ;
	org_amiga_image_width = width;
	org_amiga_image_height = height;

//	printf("%s:%d - format %d\n",__FUNCTION__,__LINE__,format);

#ifdef __copy_to_vram__

	for (n=0;n<num_buffers;n++)
	{

		ram_bitmap[n] = IGraphics->AllocBitMapTags(width, height  , 32,
					BMATags_UserPrivate, TRUE,
					BMATags_PixelFormat, format,
					TAG_DONE);

		if (ram_bitmap[n])
		{
			lock = IGraphics->LockBitMapTags(ram_bitmap[n],	LBM_PlanarYUVInfo, &yuvInfo,	TAG_END);

			if (lock != NULL)
			{
				bzero( yuvInfo.YMemory , (amiga_image_height ) * yuvInfo.YBytesPerRow );
				clear_plain_half( height, yuvInfo.UMemory, 127, yuvInfo.UBytesPerRow );
				clear_plain_half( height, yuvInfo.VMemory, 127, yuvInfo.VBytesPerRow );
				IGraphics->UnlockBitMap(lock);
			}
		}
	}

#endif

//	printf("%s:%d - format %d\n",__FUNCTION__,__LINE__,format);

	vram_bitmap = IGraphics->AllocBitMapTags(width, height , 32,
					BMATags_Displayable, TRUE,
					BMATags_PixelFormat, format,
					TAG_DONE);

	hookData.srcBitMap = vram_bitmap;

}

static void free_output_buffer( void )
{
	int n;

//	printf("%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);

	for (n=0;n<num_buffers;n++)
	{
		if (ram_bitmap[n])
		{
			IGraphics->FreeBitMap(ram_bitmap[n]);
			ram_bitmap[n] = NULL;
		}
	}

	if (vram_bitmap)
	{
		IGraphics->FreeBitMap(vram_bitmap);
		vram_bitmap = NULL;
	}
}

// A YUV colour value
typedef struct YUV_s {
	int y;
	int u;
	int v;
} YUV_t;

// An RGB colour value
typedef struct RGB_s {
	uint8 r;
	uint8 g;
	uint8 b;
} RGB_t;

// Convert an RGB colour value to a YUV colour value.
static inline void RGBtoYUV (RGB_t rgb, YUV_t *yuv)
{
    yuv->y = 0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b;
    yuv->u = (rgb.b - yuv->y) * 0.565 + 128;
    yuv->v = (rgb.r - yuv->y) * 0.713 + 128;
}


static void no_clip_composite(struct RastPort *rastPort);
static void set_target_hookData( void );

static void no_clip_composite(struct RastPort *rastPort)
{
	IGraphics->CompositeTags(
		COMPOSITE_Src_Over_Dest, hookData.srcBitMap, rastPort -> BitMap,
		COMPTAG_SrcWidth,   hookData.srcWidth,
		COMPTAG_SrcHeight,  hookData.srcHeight,
		COMPTAG_ScaleX, 	hookData.scaleX,
		COMPTAG_ScaleY, 	hookData.scaleY,
		COMPTAG_OffsetX,    My_Window->LeftEdge,
		COMPTAG_OffsetY,    My_Window->TopEdge,
		COMPTAG_DestX,      My_Window->LeftEdge,
		COMPTAG_DestY,      My_Window->TopEdge, 
		COMPTAG_DestWidth,  My_Window->Width,
		COMPTAG_DestHeight, My_Window->Height,
		COMPTAG_Flags,      COMPFLAG_SrcFilter | COMPFLAG_IgnoreDestAlpha | COMPFLAG_HardwareOnly,
		TAG_END);
}

static ULONG compositeHookFunc(struct Hook *hook, struct RastPort *rastPort, struct BackFillMessage *msg) {
	CompositeHookData *hookData = (CompositeHookData*)hook->h_Data;

	hookData->retCode = IGraphics->CompositeTags(
		COMPOSITE_Src_Over_Dest, hookData->srcBitMap, rastPort->BitMap,
		COMPTAG_SrcWidth,   hookData->srcWidth,
		COMPTAG_SrcHeight,  hookData->srcHeight,
		COMPTAG_ScaleX, 	hookData->scaleX,
		COMPTAG_ScaleY, 	hookData->scaleY,
		COMPTAG_OffsetX,    msg->Bounds.MinX - (msg->OffsetX - hookData->offsetX),
		COMPTAG_OffsetY,    msg->Bounds.MinY - (msg->OffsetY - hookData->offsetY),
		COMPTAG_DestX,      msg->Bounds.MinX,
		COMPTAG_DestY,      msg->Bounds.MinY,
		COMPTAG_DestWidth,  msg->Bounds.MaxX - msg->Bounds.MinX + 1,
		COMPTAG_DestHeight, msg->Bounds.MaxY - msg->Bounds.MinY + 1,
		COMPTAG_Flags,      COMPFLAG_SrcFilter | COMPFLAG_IgnoreDestAlpha | COMPFLAG_HardwareOnly,
		TAG_END);

	return 0;
}

static void set_target_hookData( void )
{
 	rect.MinX = My_Window->BorderLeft;
 	rect.MinY = My_Window->BorderTop;
 	rect.MaxX = My_Window->Width - My_Window->BorderRight - 1;
 	rect.MaxY = My_Window->Height - My_Window->BorderBottom - 1;

 	float destWidth = rect.MaxX - rect.MinX + 1;
 	float destHeight = rect.MaxY - rect.MinY + 1;
 	float scaleX = (destWidth + 0.5f) / amiga_image_width;
 	float scaleY = (destHeight + 0.5f) / amiga_image_height;

	hookData.srcWidth = amiga_image_width;
	hookData.srcHeight = amiga_image_height;
	hookData.offsetX = My_Window->BorderLeft;
	hookData.offsetY = My_Window->BorderTop;
	hookData.scaleX = COMP_FLOAT_TO_FIX(scaleX);
	hookData.scaleY = COMP_FLOAT_TO_FIX(scaleY);
	hookData.retCode = COMPERR_Success;

	hook.h_Entry = (HOOKFUNC)compositeHookFunc;
	hook.h_Data = &hookData;
}

static void BackFill_Func(struct RastPort *ArgRP, struct BackFillArgs *MyArgs)
{
	set_target_hookData();

	IExec->MutexObtain( window_mx );
	if ((vram_bitmap)&&(My_Window))
	{
		register struct RastPort *RPort = My_Window->RPort;

		ILayers->LockLayer(0, RPort->Layer);
		ILayers->DoHookClipRects(&hook, RPort, &rect);
		ILayers->UnlockLayer( RPort->Layer);
	}
	IExec->MutexRelease(window_mx);
}


struct Window *bf_Window = NULL;

// not OS specific

static uint32_t   window_width;           // width and height on the window
static uint32_t   window_height;          // can be different from the image

extern uint32_t is_fullscreen;
extern uint32_t can_go_faster;

static BOOL	is_my_screen;
static BOOL	is_paused = FALSE;

extern UWORD *EmptyPointer;               // Blank pointer
extern ULONG WantedModeID;

static uint32_t   offset_x;               	// offset in the rp where we have to display the image
static uint32_t   offset_y;               	// ...

static uint32_t		amiga_image_format;

static uint32_t   win_left;             
static uint32_t   win_top;              

static SwsContext *swsContext=NULL;

/***************************************/
struct BackFillArgs
{
	struct Layer     *layer;
	struct Rectangle  bounds;
	LONG              offsetx;
	LONG              offsety;
};

static struct Hook BackFill_Hook =
{
	{NULL, NULL},
	(HOOKFUNC) &BackFill_Func,
	NULL,
	NULL
};

#ifdef __GNUC__
# pragma pack(2)
#endif

#ifdef __GNUC__
# pragma pack()
#endif


/******************************** DRAW ALPHA ******************************************/

static void draw_alpha_yv12 (int x0,int y0, int w,int h, unsigned char* src, unsigned char * srca, int stride)
{
	struct PlanarYUVInfo yuvInfo;
	APTR lock;

#ifdef __copy_to_vram__xxx
	if (!ram_bitmap[current_buf]) return;
	lock = IGraphics->LockBitMapTags(ram_bitmap[current_buf],LBM_PlanarYUVInfo, &yuvInfo,	TAG_END);
#else
	if (!vram_bitmap) return;
	lock = IGraphics->LockBitMapTags(vram_bitmap,LBM_PlanarYUVInfo, &yuvInfo,	TAG_END);
#endif

	if (lock != NULL)
	{
		vo_draw_alpha_yv12 ( w, h,  src, srca, stride,
			(UBYTE *) ( (ULONG) yuvInfo.YMemory + (y0*yuvInfo.YBytesPerRow)+x0 ), yuvInfo.YBytesPerRow );

		IGraphics->UnlockBitMap(lock);
	}
}

extern struct Library *GraphicsBase;
// extern struct Library *LayersBase;

static uint32 vsync_is_enabled = 0;

/******************************** PREINIT ******************************************/
static int preinit(const char *arg)
{
	BOOL have_compositing = FALSE;
	BOOL have_readeonhd = FALSE;
	BOOL have_bitmap_format = FALSE;
	struct BitMap *test_bitmap;

	if ( ! LIB_IS_AT_LEAST(GraphicsBase, 54,100) )
	{
		mp_msg(MSGT_VO, MSGL_INFO, "VO: [comp_yuv2] You need graphics.library version 54.100 or newer.\n");	
		return -1;
	}

	if ( ! LIB_IS_AT_LEAST(LayersBase, 54,2) )
	{
		mp_msg(MSGT_VO, MSGL_INFO, "VO: [comp_yuv2] You need layers.library version 54.2 or newer.\n");	
		return -1;
	}

	mp_msg(MSGT_VO, MSGL_INFO, "VO: [comp_yuv2] Welcome man !.\n");

	if (!gfx_GiveArg(arg))
	{
		mp_msg(MSGT_VO, MSGL_INFO, "VO: [comp_yuv2] config is wrong\n");	
		return -1;	
	}

	printf("gfx_nodri = %d\n",(int) gfx_nodri);


	if (gfx_nodri==1)
	{
		vo_directrendering = 0;
	}
	else
	{
		vo_directrendering = 1;
	}

	if ( ( the_screen = IIntuition->LockPubScreen ( gfx_screenname  ) ) )
	{
		ULONG DisplayID = 0;
		struct DisplayInfo display_info;
		char *ChipDriverName = 0;

		is_my_screen = FALSE;

		IIntuition->GetScreenAttr( the_screen, SA_DisplayID, &DisplayID , sizeof(DisplayID)  );
		IGraphics->GetDisplayInfoData( NULL, &display_info, sizeof(display_info) , DTAG_DISP, DisplayID);
		IGraphics->GetBoardDataTags( display_info.RTGBoardNum, GBD_ChipDriver, &ChipDriverName, TAG_END);

		if (strcasecmp(ChipDriverName,"RadeonHD.chip") == 0 )
		{
			have_readeonhd = TRUE;
		}

		best_screen_aspect_ratio = (float) the_screen -> Width / (float) the_screen -> Height;

//IGraphics->AllocBitMap( 200 , 200, 32, BMF_DISPLAYABLE, &the_screen -> BitMap)

		if (test_bitmap =  IGraphics->AllocBitMapTags(200, 200  , 32,	
							BMATags_Displayable, TRUE,	
							BMATags_PixelFormat, PIXF_YUV420P, 
							BMATags_Friend, &the_screen -> BitMap,
							TAG_DONE))
		{
			have_bitmap_format = TRUE;

			if ( COMPERR_Success == IGraphics->CompositeTags(
					COMPOSITE_Src, test_bitmap, test_bitmap,
					COMPTAG_Flags,      COMPFLAG_SrcFilter | COMPFLAG_IgnoreDestAlpha | COMPFLAG_HardwareOnly,
					TAG_END))
			{
				have_compositing = TRUE;
			}

			IGraphics->FreeBitMap(test_bitmap);
		}

		IIntuition->UnlockPubScreen(NULL, the_screen);
	}

	if (have_bitmap_format == FALSE)
	{
		mp_msg(MSGT_VO, MSGL_INFO, "VO: [comp_yuv2] Sceeen does not support the bitmap format.\n");
		return -1;
	}

	if (have_readeonhd == FALSE)
	{
		mp_msg(MSGT_VO, MSGL_INFO, "VO: [comp_yuv2] You need RadeonHD to use this video output.\n");
		return -1;
	}

	if (have_compositing == FALSE)
	{
		mp_msg(MSGT_VO, MSGL_INFO, "VO: [comp_yuv2] Sceeen mode can't do hardware compositing.\n");	
		return -1;
	}

	printf("gfx_novsync = %d\n", (int) gfx_novsync);
	printf("benchmark = %d\n", benchmark);

	vsync_is_enabled = ((benchmark == 0) && (gfx_novsync == 0)) ? 1 : 0;
	vo_vsync = vsync_is_enabled;

	printf("vsync is enabled = %d\n", (int) vsync_is_enabled);

	benchmark_frame_cnt = 0;
	gettimeofday(&before,&dontcare);		// to get time before

	StartVOProcess();
	return 0;
}

extern char * filename;

static ULONG Open_Window( void );
static ULONG Open_FullScreen( void );
static void voprocess( void );

static ULONG Open_Window()
{		
	// Window
	ULONG ModeID = INVALID_ID;
	BOOL WindowActivate = TRUE;

	My_Window = NULL;

//	dprintf("%s:%ld\n",__FUNCTION__,__LINE__);

	if ( ( the_screen = IIntuition->LockPubScreen ( gfx_screenname ) ) )
	{
		is_my_screen = FALSE;
		ModeID =  IGraphics->GetVPModeID(&the_screen->ViewPort);

		if (force_monitor_aspect)
		{
			printf("**\nyour forceing aspect to %0.2f:1\n",force_monitor_aspect );
			best_screen_aspect_ratio = force_monitor_aspect;
			printf("WARNING: workbench screen resolution suggests %0.2f:1\n", (float) the_screen -> Width / (float) the_screen -> Height);

		}
		else
		{
			printf("screen w %d h %d aspect %0.2f:1\n", the_screen -> Width, the_screen -> Height, (float) the_screen -> Width / (float) the_screen -> Height);
			best_screen_aspect_ratio = (float) the_screen -> Width / (float) the_screen -> Height;
		}



		if (FirstTime)
		{
			gfx_center_window(the_screen, window_width, window_height, &win_left, &win_top);
		}

		switch(gfx_BorderMode)
		{
			case NOBORDER:
				My_Window = IIntuition->OpenWindowTags( NULL,
					WA_CustomScreen,    (ULONG) the_screen,
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
				My_Window = IIntuition->OpenWindowTags( NULL,
					WA_CustomScreen,    (ULONG) the_screen,
					WA_Left,            win_left,
					WA_Top,             win_top,
					WA_InnerWidth,      window_width,
					WA_InnerHeight,     window_height,
					WA_SimpleRefresh,   TRUE,
					WA_CloseGadget,     FALSE,
					WA_DepthGadget,     FALSE,
					WA_DragBar,         FALSE,
					WA_Borderless,      FALSE,
					WA_SizeGadget,      TRUE,
					WA_NewLookMenus,	TRUE,
					WA_Activate,        WindowActivate,
					WA_IDCMP,           IDCMP_COMMON,
					WA_Flags,           WFLG_REPORTMOUSE,
					//WA_SkinInfo,				NULL,
					TAG_DONE);	
				break;

			default:
				My_Window = IIntuition->OpenWindowTags( NULL,
					WA_CustomScreen,    (ULONG) the_screen,
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
					WA_MinWidth, 		(window_width/3) > 100 ? window_width/3 : 100,
					WA_MinHeight,		(window_height/3) > 100 ? window_height/3 : 100,
					WA_MaxWidth, 		the_screen -> Width,
					WA_MaxHeight,		the_screen -> Height,
					WA_SimpleRefresh,		TRUE,
					WA_CloseGadget,     TRUE,
					WA_DepthGadget,     TRUE,
					WA_DragBar,         TRUE,
					WA_Borderless,      (gfx_BorderMode == NOBORDER) ? TRUE : FALSE,
					WA_SizeGadget,      TRUE,
					WA_SizeBBottom,	TRUE,
					WA_NewLookMenus,	TRUE,
					WA_Activate,        WindowActivate,
					WA_IDCMP,           IDCMP_COMMON,
					WA_Flags,           WFLG_REPORTMOUSE,
					//WA_SkinInfo,        NULL,
					TAG_DONE);
		}

		vo_fs = 0;

		vo_screenwidth = the_screen -> Width;
		vo_screenheight = the_screen -> Height;

		IIntuition->UnlockPubScreen(NULL, the_screen);
	}


		EmptyPointer = IExec->AllocVec(16, MEMF_PUBLIC | MEMF_CLEAR);

	if ( !My_Window || !EmptyPointer)
	{
		mp_msg(MSGT_VO, MSGL_ERR, "Unable to open a window\n");

		if (gfx_screenname)
		{
			free(gfx_screenname); 
			gfx_screenname = NULL;
		}

		uninit();
		return INVALID_ID;
	}

	offset_x = (gfx_BorderMode == NOBORDER) ? 0 : My_Window->BorderRight;	
	offset_y = (gfx_BorderMode == NOBORDER) ? 0 : My_Window->BorderTop;

	if ( (ModeID = IGraphics->GetVPModeID(&My_Window->WScreen->ViewPort) ) == INVALID_ID) 
	{
		uninit();
		return INVALID_ID;
	}

	IIntuition->ScreenToFront(My_Window->WScreen);

	gfx_StartWindow(My_Window);
	gfx_ControlBlanker(the_screen, FALSE);


	return ModeID;
}


static ULONG Open_FullScreen()
{ 
	// if fullscreen -> let's open our own screen
	// It is not a very clean way to get a good ModeID, but a least it works
	struct Screen *Screen;
	ULONG ModeID;

	ULONG left = 0, top = 0;
	ULONG out_width = 0;
	ULONG out_height = 0;

	if(WantedModeID)
	{
		ModeID = WantedModeID;
	}
	else
	{
		if ( ! ( Screen = IIntuition->LockPubScreen(NULL) ) ) 
		{
			uninit();
			return INVALID_ID;
		}

		ModeID = IGraphics->GetVPModeID(&Screen->ViewPort);
		IIntuition->UnlockPubScreen(NULL, Screen);
	}

	if ( ModeID == INVALID_ID) 
	{
		uninit();
		return INVALID_ID;
	}

	ModeID = find_best_screenmode( gfx_monitor, 32, best_screen_aspect_ratio, amiga_image_width, amiga_image_height);
	if (ModeID == INVALID_ID)
	{
		// use first aspect found if no modes where found.
		ModeID = find_best_screenmode( gfx_monitor, 32, -1.0f, amiga_image_width, amiga_image_height);
	}

	print_screen_info( gfx_monitor, ModeID );

	if (ModeID != INVALID_ID)
	{
		the_screen = IIntuition ->OpenScreenTags ( NULL,
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

	 if ( ! the_screen ) 
	{
		mp_msg(MSGT_VO, MSGL_ERR, "Unable to open the screen ID:%x\n", (int) ModeID);
		uninit();
		return INVALID_ID;
	}

	is_my_screen = TRUE;

	dprintf("%s:%ld\n",__FUNCTION__,__LINE__);

	vo_screenwidth = the_screen -> Width;
	vo_screenheight = the_screen -> Height;

	offset_x = (vo_screenwidth - window_width)/2;
	offset_y = (vo_screenheight - window_height)/2;

	/* calculate new video size/aspect */
	aspect_save_screenres(the_screen->Width,the_screen->Height);

	out_width = amiga_image_width;
	out_height = amiga_image_height;

   	aspect(&out_width,&out_height,A_ZOOM);

	left=(the_screen->Width-out_width)/2;
	top=(the_screen->Height-out_height)/2;

	bf_Window = IIntuition->OpenWindowTags( NULL,
			WA_PubScreen,       (ULONG) the_screen,
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
		IGraphics->RectFillColor(bf_Window->RPort, 0,0, the_screen->Width, the_screen->Height, 0x00000000);
	}

	My_Window = IIntuition->OpenWindowTags( NULL,
			WA_PubScreen,       (ULONG) the_screen,
			WA_Top,		top,
			WA_Left,		left,
			WA_Height,		out_height,
			WA_Width,		out_width,
			WA_SimpleRefresh,   TRUE,
			WA_CloseGadget,     FALSE,
			WA_DragBar,         FALSE,
			WA_Borderless,      TRUE,
			WA_Backdrop,        FALSE,
			WA_NewLookMenus,	TRUE,
			WA_Activate,        TRUE,
			WA_IDCMP,           IDCMP_MOUSEMOVE | IDCMP_MENUPICK | IDCMP_MENUVERIFY | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | 
							IDCMP_EXTENDEDMOUSE | IDCMP_REFRESHWINDOW | IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW,
			WA_Flags,           WFLG_REPORTMOUSE,
		TAG_DONE);

	if ( ! My_Window) 
	{
		mp_msg(MSGT_VO, MSGL_ERR, "Unable to open a window\n");
		uninit();
		return INVALID_ID;
	}

	IIntuition->SetWindowAttrs(My_Window, WA_Left,left, WA_Top,top, WA_Width,out_width, WA_Height,out_height, TAG_DONE);

	IDOS->Printf("Screen w %ld h %ld\n",the_screen->Width,the_screen->Height);

	vo_fs = 1;

	gfx_ControlBlanker(the_screen, FALSE);

	return ModeID;
} 

extern struct {  const char* name;  unsigned int fmt; } *mp_imgfmt_list ;

static int PrepareBuffer(uint32_t in_format, uint32_t out_format)
{
	switch (in_format)
	{
		case IMGFMT_YV12:
			vo_format = IMGFMT_YV12;
			gfx_common_rgb_format = PIXF_YUV420P;
			draw_alpha_func =(vo_draw_alpha_func) draw_alpha_yv12;
			break;

		case IMGFMT_I420:
			vo_format = IMGFMT_I420;
			gfx_common_rgb_format = PIXF_YUV420P;
			draw_alpha_func =(vo_draw_alpha_func) draw_alpha_yv12;
			break;

		case IMGFMT_NV12:
			vo_format = IMGFMT_NV12;
			gfx_common_rgb_format = PIXF_YUV420P;
			draw_alpha_func =(vo_draw_alpha_func) draw_alpha_yv12;
			break;
/*
		case IMGFMT_422P:
			vo_format = IMGFMT_422P;
			gfx_common_rgb_format = PIXF_YUV422;
			draw_alpha_func =(vo_draw_alpha_func) draw_alpha_yv12;
			break;
*/
		default:

			IDOS->Printf("\n\nSorry not supported video mode\n\n\n");
			vo_format = 0;
			gfx_common_rgb_format = 0;
			draw_alpha_func = NULL;
	} 

	return 0;
}


/******************************** CONFIG ******************************************/
static int config(uint32_t width, uint32_t height, uint32_t d_width,
		     uint32_t d_height, uint32_t flags, char *title,
		     uint32_t in_format)
{
	ModeID = INVALID_ID;

	if (My_Window) return 0;

	is_fullscreen = flags & VOFLAG_FULLSCREEN;
	set_gfx_rendering_option();

	is_paused = FALSE;

	num_buffers = vo_doublebuffering ? (vo_directrendering ? NUM_BUFFERS : 2) : 1;

	amiga_image_format = in_format; 

	if (FirstTime)
	{
		gfx_common_rgb_format = PIXF_YUV420P;
		alloc_output_buffer( width, height, gfx_common_rgb_format );
		vo_dwidth = amiga_image_width;
		vo_dheight = amiga_image_height;
	}

	IExec->MutexObtain( window_mx );

	PrepareBuffer(in_format, 0 );

	if (vo_process)
	{
		arg_d_width = d_width;
		arg_d_height = d_height;
		IExec->Signal( &vo_process->pr_Task, (1<<sig_vo_openwindow) );
		IExec->Wait(SIGF_CHILD);		// Wait for process to signal to continue
	}

	if (!My_Window)
	{
		while ((window_width<300)||(window_height<300))
		{
			window_width *= 2;
			window_height *= 2;
		}

		IExec->MutexRelease(window_mx);
		uninit();
		return -1;
	}

	FirstTime = FALSE;

	IExec->MutexRelease(window_mx);

	gfx_Start(My_Window);
	flip_page();	// update the gfx, on slow FPS.

	return 0; // -> Ok
}


void clear_plain_half( int h, char *memory, ULONG value, uint32 BytesPerRow)
{
	register int y;
	int BytesPerRow_h = BytesPerRow >>1;

	h/=2;

	if (memory)
	{
		if (h&3==0)
		{
			h = h>>2;
			for (y=0;y<h;y++)
			{
				memset( (void *) (memory) , value , BytesPerRow_h);
				memory += BytesPerRow;
				memset( (void *)  (memory) , value , BytesPerRow_h);
				memory += BytesPerRow;
				memset( (void *) (memory) , value , BytesPerRow_h);
				memory += BytesPerRow;
				memset( (void *) (memory) , value , BytesPerRow_h);
				memory += BytesPerRow;
			}
			return;
		}
		else
		{
			for (y=0;y<h;y++)
			{
				memset( (void *) ((uint32) (memory) + (y * BytesPerRow)) , value , BytesPerRow/2);
			}
			return;
		}
	}
}
void copy_plain_half(int yf, int w, register int h, register char *src, int bpr, register char *dst, uint32 BytesPerRow)
{
	register int y;
	register int size;

	h/=2;yf/=2;

	dst += yf * BytesPerRow;
	size = bpr>(w/2) ?  w/2 : bpr;

	if (h&3==0)
	{
		h = h>>2;
		for (y=0;y<h;y++)
		{
			memcpy( (void *) dst, (void *) src,  size );
			src += bpr;	dst += BytesPerRow;
			memcpy( (void *) dst, (void *) src,  size );
			src += bpr;	dst += BytesPerRow;
			memcpy( (void *) dst, (void *) src,  size );
			src += bpr;	dst += BytesPerRow;
			memcpy( (void *) dst, (void *) src,  size );
			src += bpr;	dst += BytesPerRow;
		}
	}
	else
	{
		for (y=0;y<h;y++)
		{
			memcpy( (void *) dst, (void *) src,  size );
			src += bpr;
			dst += BytesPerRow;
		}
	}
}

void copy_plain(int yf, int w, register  int h, register char *src, int bpr, register  char *dst, uint32 BytesPerRow)
{
	register int y;
	register int size;

	dst += yf * BytesPerRow;

	if (bpr == BytesPerRow)
	{
			size = h * bpr;
			IExec->CopyMemQuick( (void *) src , (void *) dst , size);
	}
	else
	{
		size = bpr>w ?  w : bpr;

		if (h&7==0)
		{
			h = h>>3;
			for (y=0;y<h;y++)
			{
				memcpy( (void *) dst, (void *) src,  size );
				src += bpr;
				dst += BytesPerRow;
				memcpy( (void *) dst, (void *) src,  size );
				src += bpr;
				dst += BytesPerRow;
				memcpy( (void *) dst, (void *) src,  size );
				src += bpr;
				dst += BytesPerRow;
				memcpy( (void *) dst, (void *) src,  size );
				src += bpr;
				dst += BytesPerRow;
				memcpy( (void *) dst, (void *) src,  size );
				src += bpr;
				dst += BytesPerRow;
				memcpy( (void *) dst, (void *) src,  size );
				src += bpr;
				dst += BytesPerRow;
				memcpy( (void *) dst, (void *) src,  size );
				src += bpr;
				dst += BytesPerRow;
				memcpy( (void *) dst, (void *) src,  size );
				src += bpr;
				dst += BytesPerRow;
			}
			return;
		}
		else
		{
			for (y=0;y<h;y++)
			{
				memcpy( (void *) dst , (void *) src , size);
				src += bpr;
				dst += BytesPerRow;
			}
			return;
		}
	}
}

static void copy_plain_i420(int yf, int w, register  int h, register char *src, int bpr, register  char *dst, uint32 BytesPerRow)
{
	register int y;
	register int size;

	dst += yf * BytesPerRow;

	if (bpr == BytesPerRow)
	{
			size = h * bpr;
			memcpy( (void *) dst , (void *) src , size);
	}
	else
	{
		size = bpr>w ?  w : bpr;

		for (y=0;y<h/2;y++)
		{
			memcpy( (void *) dst , (void *) src , size);
			src += bpr;
			dst += BytesPerRow;
		}
	}
}

static void copy_plain_nv12(int yf, int w, register int h, register char *src, int bpr, register char *dst1, register char *dst2, uint32 BytesPerRow)
{
	register int x,y;
	register int size;

	h/=2;yf/=2;
	w/=2;

	dst1 += yf * BytesPerRow;
	dst2 += yf * BytesPerRow;
	size = bpr>w ?  w : bpr;

	for (y=0;y<h;y++)
	{
		for (x=0;x<size;x++)
		{
			dst1[x] =src[x*2+0];
			dst2[x] =src[x*2+1];
		}

		src += bpr;
		dst1 += BytesPerRow;
		dst2 += BytesPerRow;
	}
}


static void copy_I420P_to_yuv420p(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	struct PlanarYUVInfo yuvInfo;
	APTR lock;

#ifdef __copy_to_vram__
	if (!ram_bitmap[current_buf]) return;
	lock = IGraphics->LockBitMapTags(ram_bitmap[current_buf],	LBM_PlanarYUVInfo, &yuvInfo,	TAG_END);
#else
	if (!vram_bitmap) return;
	lock = IGraphics->LockBitMapTags(vram_bitmap,	LBM_PlanarYUVInfo, &yuvInfo,	TAG_END);
#endif

	if (lock != NULL)
	{
		copy_plain(y,amiga_image_width, h,image[0],stride[0], yuvInfo.YMemory,yuvInfo.YBytesPerRow);
//		copy_plain_i420(y,amiga_image_width, h,image[1],stride[1], yuvInfo.UMemory,yuvInfo.UBytesPerRow);

		printf("%d - %d - %d\n",((uint) yuvInfo.VMemory) - ((uint) yuvInfo.UMemory) , amiga_image_width / 2, stride[1] );


//		copy_plain_half(y,amiga_image_width, h,image[1],stride[1], yuvInfo.UMemory,yuvInfo.UBytesPerRow);
//		copy_plain_half(y,amiga_image_width, h,image[2],stride[2], yuvInfo.VMemory,yuvInfo.VBytesPerRow);

		IGraphics->UnlockBitMap(lock);
	}
}


static void copy_yv12_to_yuv420p(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	struct PlanarYUVInfo yuvInfo;
	APTR lock;

if (gfx_nodma==FALSE)
{
	if (!ram_bitmap[current_buf]) return;
	lock = IGraphics->LockBitMapTags(ram_bitmap[current_buf],	LBM_PlanarYUVInfo, &yuvInfo,	TAG_END);
} else {
	if (!vram_bitmap) return;
	lock = IGraphics->LockBitMapTags(vram_bitmap,	LBM_PlanarYUVInfo, &yuvInfo,	TAG_END);
}

	if (lock != NULL)
	{
		copy_plain(y,amiga_image_width, h,image[0],stride[0], yuvInfo.YMemory,yuvInfo.YBytesPerRow);
		copy_plain_half(y,amiga_image_width, h,image[1],stride[1], yuvInfo.UMemory,yuvInfo.UBytesPerRow);
		copy_plain_half(y,amiga_image_width, h,image[2],stride[2], yuvInfo.VMemory,yuvInfo.VBytesPerRow);

		IGraphics->UnlockBitMap(lock);
	}
}

static void copy_nv12_to_yuv420p(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	struct PlanarYUVInfo yuvInfo;
	APTR lock;

#ifdef __copy_to_vram__
	if (!ram_bitmap[current_buf]) return;
	lock = IGraphics->LockBitMapTags(ram_bitmap[current_buf],	LBM_PlanarYUVInfo, &yuvInfo,	TAG_END);
#else
	if (!vram_bitmap) return;
	lock = IGraphics->LockBitMapTags(vram_bitmap,	LBM_PlanarYUVInfo, &yuvInfo,	TAG_END);
#endif

	if (lock != NULL)
	{
		copy_plain(y,amiga_image_width, h,image[0],stride[0], yuvInfo.YMemory,yuvInfo.YBytesPerRow);

		copy_plain_nv12(y,amiga_image_width, h,image[1],stride[1], yuvInfo.UMemory,yuvInfo.VMemory,yuvInfo.UBytesPerRow);


		IGraphics->UnlockBitMap(lock);
	}
}


/******************************** DRAW_SLICE ******************************************/

static int draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
//	current_buf = visible_buf;
//	current_buf = 0; visible_buf = 0;

#ifdef __copy_to_vram__
	if (ram_bitmap[current_buf])
#else
	if (vram_bitmap)
#endif
	{
		switch (vo_format)
		{
			case IMGFMT_I420:
				copy_I420P_to_yuv420p(image, stride,w,h, x,y);
				break;
			case IMGFMT_YV12:
				copy_yv12_to_yuv420p(image, stride,w,h, x,y);
				break;
			case IMGFMT_NV12:
				copy_nv12_to_yuv420p(image, stride,w,h, x,y);
				break;
		}
	}

	return 0;
}
/******************************** DRAW_OSD ******************************************/

static void draw_osd(void)
{
	if (draw_alpha_func) vo_draw_text(amiga_image_width,amiga_image_height, (void *) draw_alpha_func);

	if (vo_process)
	{
		IExec->Signal( &vo_process->pr_Task, (1<<sig_vo_osd) );
		IExec->Wait(SIGF_CHILD);	// just becouse some other process need to wake up first.
	}
}

/******************************** FLIP_PAGE ******************************************/
static void flip_page(void)
{
	if (vo_process)
	{
		IExec->Signal( &vo_process->pr_Task, (1<<sig_vo_pageflip) );
		IExec->Wait(SIGF_CHILD);	// just becouse some other process need to wake up first.
	}
}
/******************************** DRAW_FRAME ******************************************/
static int draw_frame(uint8_t *src[])
{
	return VO_ERROR;
}


/***********************/
// Just a litle func to help uninit and control
// it close screen (if fullscreen), the windows, and free all gfx related stuff
// but do not clsoe any libs

static void FreeGfx(void)
{
	gfx_ControlBlanker(the_screen, TRUE);
	gfx_Stop(My_Window);

	dprintf("%s:%s:%ld\n",__FILE__,__FUNCTION__,__LINE__);

	if (vo_process)
	{
		IExec->Signal( &vo_process->pr_Task, (1<<sig_vo_closewindow) );
		IExec->Wait(SIGF_CHILD);		// Wait for process to signal to continue
	}

	dprintf("%s:%s:%ld\n",__FILE__,__FUNCTION__,__LINE__);
}

/******************************** UNINIT    ******************************************/
static void uninit(void)
{
	unsigned long long int microsec;	

	if (benchmark_frame_cnt>0)
	{
		gettimeofday(&after,&dontcare);		// to get time after
		microsec = (after.tv_usec - before.tv_usec) +(1000000 * (after.tv_sec - before.tv_sec));
		printf("Internal COMP YUV2 - FPS %0.00f\n",(double)  benchmark_frame_cnt /  ( (double) microsec / 1000.0l / 1000.0l) );
	}
	benchmark_frame_cnt = 0;

	// close window and stop process.
	FreeGfx();
	StopVOProcess();

	// bitmap not need any more now the window is closed.
	free_output_buffer();

	FirstTime = TRUE;

	if (EmptyPointer)
	{
	  IExec->FreeVec(EmptyPointer);
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
			set_gfx_rendering_option();

			FreeGfx();
			if ( config(org_amiga_image_width, org_amiga_image_height, window_width, window_height, is_fullscreen, NULL, amiga_image_format) < 0) return VO_FALSE;
			return VO_TRUE;

		case VOCTRL_PAUSE:

			gfx_Stop(My_Window);
			ILayers->InstallLayerHook(My_Window->RPort->Layer, &BackFill_Hook);

			gfx_ControlBlanker(the_screen, TRUE);

			is_paused = is_paused ? FALSE : TRUE;

			BackFill_Func( My_Window -> RPort, NULL );
			return VO_TRUE;					

		case VOCTRL_RESUME:

			gfx_Start(My_Window);

			ILayers->InstallLayerHook(My_Window->RPort->Layer, NULL );

			gfx_ControlBlanker(the_screen, FALSE);

			is_paused = is_paused ? FALSE : TRUE;

			BackFill_Func( My_Window -> RPort, NULL );
			return VO_TRUE;

		case VOCTRL_QUERY_FORMAT:
			return query_format(*(ULONG *)data);

		case VOCTRL_GET_IMAGE:
			if (gfx_nodri==1)
			{
				printf("** Codec asked, but was not allowed **\n");
				return VO_FALSE;
			}
			else
			{
				return get_image(data);
			}
			break;

	       	case VOCTRL_DRAW_IMAGE:
			if (gfx_nodri==1)
			{
				printf("** Codec asked, but was not allowed **\n");
				return VO_FALSE;
			}
			else
			{
				return draw_image(data);
			}
			break;

		case VOCTRL_GET_PANSCAN:
			return VO_TRUE;

		case VOCTRL_ONTOP:
			return VO_TRUE;

//		case VOCTRL_UPDATE_SCREENINFO:
//			return VO_TRUE;

		case VOCTRL_GET_EQUALIZER:
			return VO_FALSE;

		case VOCTRL_SET_EQUALIZER:
			return VO_FALSE;
	}
  
	return VO_NOTIMPL;
}

/******************************** CHECK_EVENTS    ******************************************/
static void check_events(void)
{
	if (vo_process)
	{
		IExec->Signal( &vo_process->pr_Task, (1<<sig_vo_events) );
		IExec->Wait(SIGF_CHILD);	// just becouse some other process need to wake up first.
	}
}

static int query_format(uint32_t format)
{
    int flag = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN | VFCAP_OSD | VFCAP_ACCEPT_STRIDE;       // FIXME! check for DOWN

//	printf("query_format(uint32_t format)\n");
//	printf("format %d is hardware acclerated %d\n", format, IMGFMT_IS_HWACCEL(format) );

	switch( format) 
	{
		case IMGFMT_YV12:
//		case IMGFMT_IYUV:
//		case IMGFMT_I420:
//		case IMGFMT_411P:
//		case IMGFMT_422P:
//		case IMGFMT_NV12:

			return flag;
		default:
			return 0;
	}
}


static uint32_t draw_image(mp_image_t * mpi)
{
    if (mpi->flags & MP_IMGFLAG_DIRECT)
    {
        // direct rendering:
        current_buf = (int) (intptr_t) (mpi->priv);        // hack
        return VO_TRUE;
    }
    if (mpi->flags & MP_IMGFLAG_DRAW_CALLBACK)
        return VO_TRUE;         // done
    if (mpi->flags & MP_IMGFLAG_PLANAR)
    {
//	printf(" if (mpi->flags & MP_IMGFLAG_DRAW_CALLBACK)\n");
	current_buf = (int) (intptr_t) (mpi->priv);
        draw_slice(mpi->planes, mpi->stride, mpi->w, mpi->h, 0, 0);
        return VO_TRUE;
    }
    if (mpi->flags & MP_IMGFLAG_YUV)
    {

//	printf(" if (mpi->flags & MP_IMGFLAG_YUV)\n");

/*
        // packed YUV:
        memcpy_pic(xvimage[current_buf]->data +
                   xvimage[current_buf]->offsets[0], mpi->planes[0],
                   mpi->w * (mpi->bpp / 8), mpi->h,
                   xvimage[current_buf]->pitches[0], mpi->stride[0]);
*/
        return VO_TRUE;
    }
    return VO_FALSE;            // not (yet) supported
}


static uint32_t get_image(mp_image_t * mpi)
{
	struct PlanarYUVInfo yuvInfo;
	APTR lock;

//	printf("should not be called\n");

	if (mpi->imgfmt != amiga_image_format) return VO_FALSE;  
	if (mpi->height > amiga_image_height) return VO_FALSE;     

	if (mpi->flags & (MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_ACCEPT_ALIGNED_STRIDE | MP_IMGFLAG_ACCEPT_WIDTH))
	{
		if (! mpi->planes[0] )
		{
			if (ram_bitmap[buffer_allocated])
			{
				lock = IGraphics->LockBitMapTags(ram_bitmap[buffer_allocated],	LBM_PlanarYUVInfo, &yuvInfo,	TAG_END);
				if (lock) IGraphics->UnlockBitMap(lock);
			}
			else
			{
				printf("** Can lock buffer %d ***\n", buffer_allocated );
				return VO_FALSE;
			}

			mpi->planes[0] = yuvInfo.YMemory;
			mpi->stride[0] = yuvInfo.YBytesPerRow;
			mpi->width = mpi->stride[0] / (mpi->bpp / 8);

			if (mpi->flags & MP_IMGFLAG_PLANAR)
			{
				if (!mpi->flags & MP_IMGFLAG_SWAPPED)
				{
					// I420
					mpi->planes[1] = yuvInfo.VMemory;
					mpi->planes[2] = yuvInfo.UMemory;

					// FFMPEG does not like BytesPerRow = Image.BytesPerRow,
					// but we have hacked FFMPEG.

					mpi->stride[1] = yuvInfo.VBytesPerRow;
					mpi->stride[2] = yuvInfo.UBytesPerRow;
				} else
				{
					// YV12

					mpi->planes[1] = yuvInfo.UMemory;
					mpi->planes[2] = yuvInfo.VMemory;

					// FFMPEG does not like BytesPerRow = Image.BytesPerRow,
					// but we have hacked FFMPEG.

					mpi->stride[1] = yuvInfo.VBytesPerRow;
					mpi->stride[2] = yuvInfo.UBytesPerRow;
				}
			}

			mpi->flags |= MP_IMGFLAG_DIRECT;
			mpi->priv = (void *)(intptr_t) buffer_allocated;

			printf("from %08x to %08x\n",mpi->planes[0], (ULONG) mpi->planes[0] + (ULONG) (mpi->stride[0] * mpi->height) );
			printf("from %08x to %08x\n",mpi->planes[1], (ULONG) mpi->planes[1] + (ULONG) (mpi->stride[1] * (mpi->height/2)) );
			printf("from %08x to %08x\n",mpi->planes[2], (ULONG) mpi->planes[2] + (ULONG) (mpi->stride[2] * (mpi->height/2)) );

			printf("VO: [comp_yuv2] allocated DR buffer %d\n",buffer_allocated);
			buffer_allocated = (buffer_allocated + 1) % num_buffers;
		}
		else
		{
			// we already have set this mpi to a buffer, we only need to reset the DIRECT flag.
			mpi->flags |= MP_IMGFLAG_DIRECT;
		}

		return VO_TRUE;
	}

	return VO_FALSE;
}

// To serialize screen open & close, and prevent mutex locks in page flip.
// and to keep main code in mplayer runing while waiting for VSync. 

static void voprocess()
{
	ULONG sigs,rsigs;

	sig_vo_osd = IExec->AllocSignal(-1);
	sig_vo_pageflip = IExec->AllocSignal(-1);
	sig_vo_openwindow = IExec->AllocSignal(-1);
	sig_vo_closewindow = IExec->AllocSignal(-1);
	sig_vo_events = IExec->AllocSignal(-1);

	sigs = 1L <<sig_vo_pageflip | 1L <<sig_vo_openwindow | 1L << sig_vo_closewindow | 1L << sig_vo_events | 1L << sig_vo_osd | SIGBREAKF_CTRL_C;

	IExec->Signal( &MainTask->pr_Task, SIGF_CHILD );

	for(;;)
	{
//		IDOS->Printf("Wait for it\n");
		rsigs = IExec->Wait( sigs  );

		if (rsigs & SIGBREAKF_CTRL_C) break;

		if (rsigs & (1<<sig_vo_openwindow))
		{
			if ( is_fullscreen )
			{
				amiga_aspect_ratio = (float) arg_d_width /  (float) arg_d_height;
 				ModeID = Open_FullScreen();
			}
		
			if (ModeID == INVALID_ID)	// so if vi failed to open on monitor.
			{
				is_fullscreen &= ~VOFLAG_FULLSCREEN;	// we do not have fullscreen
				set_gfx_rendering_option();

				if ( ( the_screen = IIntuition->LockPubScreen ( gfx_screenname) ) )
				{
					/* calculate new video size/aspect */
					aspect_save_screenres(the_screen->Width,the_screen->Height);

					window_width = arg_d_width ;
					window_height = arg_d_height;

				   	aspect(&window_width,&window_height,A_NOZOOM);

					if ((window_width < 15)|| ( window_height < 15))
					{
						window_width = arg_d_width ;
						window_height = arg_d_height;
					}

					amiga_aspect_ratio = (float) window_width /  (float) window_height;
				}

				ModeID = Open_Window();
			}

			if (My_Window) 	// if success attach menu.
			{
				attach_menu(My_Window);
				set_target_hookData();
			}


			IExec->Signal( &MainTask->pr_Task, SIGF_CHILD );	// confirm done.
		}

		if (rsigs & (1<<sig_vo_closewindow))
		{
			if (bf_Window) 
			{
				IIntuition->CloseWindow(bf_Window);
				bf_Window=NULL;
			}

			if (My_Window) 
			{
				gfx_StopWindow(My_Window);
				ILayers->InstallLayerHook(My_Window->RPort->Layer, NULL);
				detach_menu(My_Window);
				IIntuition->CloseWindow(My_Window);
				My_Window=NULL;
			}

			if (is_my_screen)
			{
				if (the_screen) 
				{
					IIntuition->CloseScreen(the_screen);
					the_screen = NULL;
				}
				is_my_screen = FALSE;
			}
			IExec->Signal( &MainTask->pr_Task, SIGF_CHILD );	// confirm done.
		}

		if (rsigs & (1<<sig_vo_events))
		{
			gfx_CheckEvents(the_screen, My_Window, &window_height, &window_width, &win_left, &win_top);
			IExec->Signal( &MainTask->pr_Task, SIGF_CHILD );	// confirm done, keep things in sync?
		}

		if (rsigs & (1<<sig_vo_osd))
		{
			visible_buf = current_buf;
			IExec->Signal( &MainTask->pr_Task, SIGF_CHILD );	// return confirm quick

			if (gfx_nodma==FALSE)
			{
				IGraphics->BltBitMap(ram_bitmap[visible_buf], 0, 0, vram_bitmap, 0, 0, amiga_image_width, amiga_image_height, 0xC0, 0xFF, NULL);
			}

			if (draw_alpha_func) vo_draw_text(amiga_image_width,amiga_image_height, (void *) draw_alpha_func);

		}

		if (rsigs & (1<<sig_vo_pageflip))
		{
			IExec->Signal( &MainTask->pr_Task, SIGF_CHILD );	// return confirm quick, we don't need to keep it waiting.

			if (My_Window)
			{
				if (vsync_is_enabled==1)
				{
					IGraphics->WaitBOVP(&(My_Window->WScreen->ViewPort));
				}

				if (can_go_faster==1)
				{
					no_clip_composite( My_Window->RPort );
//					ILayers->DoHookClipRects(&hook, My_Window->RPort, &rect);
				}
				else
				{
					set_target_hookData();
					ILayers->DoHookClipRects(&hook, My_Window->RPort, &rect);
				}
			}
			benchmark_frame_cnt++;
		}
	}

	IDOS->Printf("Free Signals\n");

	IExec->FreeSignal(sig_vo_osd);
	IExec->FreeSignal(sig_vo_pageflip);
	IExec->FreeSignal(sig_vo_openwindow);
	IExec->FreeSignal(sig_vo_closewindow);
	IExec->FreeSignal(sig_vo_events);

	IDOS->Printf("confirm quit\n");

	IExec->Signal( &MainTask->pr_Task, SIGF_CHILD );	// confirm quit.
}

static void StartVOProcess()
{
//	APTR output = IDOS->Open("CON:",MODE_NEWFILE);

	MainTask = IExec->FindTask(NULL);
	vo_process = IDOS->CreateNewProcTags(
				NP_Name, "MPlayer Video Process" ,
				NP_Entry, voprocess, 
//				NP_Output, output,
				NP_Priority, 20, 
				NP_Child, TRUE,
				TAG_END);

	IExec->Wait(SIGF_CHILD);
}


static void StopVOProcess()
{
	if (vo_process)
	{
		IExec->Signal( &vo_process->pr_Task, SIGBREAKF_CTRL_C );
		IExec->Wait(SIGF_CHILD);		// Wait for process to signal to continue
		IDOS->Delay(1);				// I just wait and hope it has quit.
		vo_process = NULL;
	}
}

