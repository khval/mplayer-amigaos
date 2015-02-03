/*
 *  vo_comp.c
 *  VO module for MPlayer AmigaOS4
 *  using Composition / direct VMEM
 *  Writen by Kjetil Hvalstrand
 *  based on work by DET Nicolas and FAB, see vo_wpa_cgx.c
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

#include "aspect.h"

//--becnhmark--
static struct timezone dontcare = { 0,0 };
static struct timeval before, after;
ULONG benchmark_frame_cnt = 0;
//------------------

/*
#include <proto/Picasso96API.h>
*/

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

//Debug
#define kk(x)
#define dprintf(...)

// OS specific


#include <proto/intuition.h>
#include <proto/graphics.h>

#include <graphics/composite.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <exec/types.h>
#include <dos/dostags.h>
#include <dos/dos.h>

struct Task *init_task = NULL;
extern APTR window_mx;

// Legacy from p96 (not p96 anymore)

struct RenderInfo
{
	char *Memory;
	ULONG BytesPerRow;
	enum enPixelFormat RGBFormat;
	ULONG pad;
};


static BOOL FirstTime = TRUE;

void draw_comp(struct BitMap *the_bitmap,struct Window * the_win, int width,int height);

static vo_info_t info =
{
	"Composition, R8G8B8",
	"comp",
	"Kjetil Hvalstrand & DET Nicolas & FAB",
	"AmigaOS4 rules da world !"
};

LIBVO_EXTERN(comp)

struct XYSTW_Vertex3D { 
float x, y; 
float s, t, w; 
}; 

void print_monitor_info()
{
	struct MonitorSpec *ms;
	if (ms = OpenMonitor(NULL,NULL))
	{
		Printf("monitor ratio: %ld, %ld\n", ms -> ratioh, ms-> ratiov);

		CloseMonitor(ms);
	}
}

struct BitMop *the_bitmap = NULL;
struct RenderInfo output_ri;

static uint32_t   org_amiga_image_width;            // well no comment
static uint32_t   org_amiga_image_height;

extern uint32_t	amiga_image_width;            // well no comment
extern uint32_t	amiga_image_height;
extern float		amiga_aspect_ratio;
static uint32_t		amiga_image_bpp;            	// image bpp


static float best_screen_aspect_ratio;

// static uint32_t get_image(mp_image_t * mpi);

void 	alloc_output_buffer( ULONG width, ULONG height )
{
	amiga_image_width = width ;
	amiga_image_height = height ;
	org_amiga_image_width = width;
	org_amiga_image_height = height;

	output_ri.BytesPerRow = amiga_image_width*amiga_image_bpp;
	output_ri.RGBFormat = gfx_common_rgb_format;
	output_ri.pad = 0;
	output_ri.Memory = AllocVec( output_ri.BytesPerRow * height+2 , MEMF_CLEAR);

}


void draw_comp_bitmap(struct BitMap *the_bitmap,struct BitMap *the_bitmap_dest, int width,int height, int wx,int wy,int ww, int wh)
{
	#define STEP(a,xx,yy,ss,tt,ww)   P[a].x= xx; P[a].y= yy; P[a].s= ss; P[a].t= tt; P[a].w= ww;  

	int error;
	struct XYSTW_Vertex3D P[6];

	STEP(0, wx, wy ,0 ,0 ,1);
	STEP(1, wx+ww,wy,width,0,1);
	STEP(2, wx+ww,wy+wh,width,height,1);

	STEP(3, wx,wy, 0,0,1);
	STEP(4, wx+ww,wy+wh,width,height,1);
	STEP(5, wx, wy+wh ,0 ,height ,1);

	if (the_bitmap)
	{

		error = CompositeTags(COMPOSITE_Src, 
			the_bitmap, the_bitmap_dest,

			COMPTAG_VertexArray, P, 
			COMPTAG_VertexFormat,COMPVF_STW0_Present,
		    	COMPTAG_NumTriangles,2,

			COMPTAG_SrcAlpha, (uint32) (0x0010000 ),
			COMPTAG_Flags, COMPFLAG_SrcAlphaOverride | COMPFLAG_HardwareOnly | COMPFLAG_SrcFilter ,
			TAG_DONE);
	}
}


void draw_comp(struct BitMap *the_bitmap,struct Window * the_win, int width,int height)
{
	draw_comp_bitmap(the_bitmap, the_win->RPort -> BitMap, width, height,
		the_win->BorderLeft + the_win -> LeftEdge,
		the_win->BorderTop + the_win -> TopEdge,
		the_win->Width - the_win->BorderLeft - the_win->BorderRight,
		the_win->Height -  the_win->BorderTop - the_win->BorderBottom);
}


static vo_draw_alpha_func draw_alpha_func;

static struct Window *         My_Window      = NULL;
static struct Screen *         My_Screen      = NULL;

static struct RastPort *       rp             = NULL;

static struct MsgPort *        UserMsg        = NULL;

// not OS specific

static uint32_t   window_width;           // width and height on the window
static uint32_t   window_height;          // can be different from the image
static uint32_t   screen_width;           // Indicates the size of the screen in full screen
static uint32_t   screen_height;          // Only use for triple buffering

extern uint32_t is_fullscreen;
BOOL	is_paused = FALSE;

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



static void BackFill_Func(struct RastPort *ArgRP, struct BackFillArgs *MyArgs);

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

struct BitMap *inner_bitmap = NULL;

static void draw_inside_window()
{
	int ww = 0;
	int wh = 0;

	if (!My_Window) return;

	MutexObtain( window_mx );

	ww = My_Window->Width - My_Window->BorderLeft - My_Window->BorderRight;
	wh = My_Window->Height - My_Window->BorderTop - My_Window->BorderBottom;
	
	if (inner_bitmap)
	{
		if ((GetBitMapAttr(inner_bitmap,BMA_WIDTH) != ww)||(GetBitMapAttr(inner_bitmap,BMA_HEIGHT) != wh))
		{
			FreeBitMap(inner_bitmap);
			inner_bitmap = NULL;
		}
	}

	if (!inner_bitmap)
	{
		inner_bitmap = AllocBitMap( ww , wh, 32, BMF_DISPLAYABLE, My_Window ->RPort -> BitMap);
	}

	if (inner_bitmap)
	{
		draw_comp_bitmap(the_bitmap, inner_bitmap, amiga_image_width, amiga_image_height,0,0,ww,wh);

		BltBitMapRastPort(inner_bitmap, 0, 0, My_Window -> RPort,
			My_Window -> BorderLeft,
			My_Window -> BorderTop,
			ww, wh,0xc0);
	}

	MutexRelease(window_mx);
}

static void BackFill_Func(struct RastPort *ArgRP, struct BackFillArgs *MyArgs)
{
	struct BitMap *bitmap;
	struct RastPort rp;
	int ww;
	int wh;

	ww = My_Window->Width - My_Window->BorderLeft - My_Window->BorderRight;
	wh = My_Window->Height - My_Window->BorderTop - My_Window->BorderBottom;

	bitmap = AllocBitMap( ww , wh, 32, BMF_DISPLAYABLE, My_Window ->RPort -> BitMap);
	if (bitmap)
	{
		InitRastPort(&rp);
		rp.BitMap = the_bitmap;

		WritePixelArray( output_ri.Memory, 0, 0, output_ri.BytesPerRow, PIXF_A8R8G8B8, &rp, 0,0, amiga_image_width,amiga_image_height);

		draw_comp_bitmap(the_bitmap, bitmap, amiga_image_width, amiga_image_height,	0,0,ww,wh);

		BltBitMapRastPort(bitmap, 0, 0, My_Window -> RPort,
			My_Window -> BorderLeft,
			My_Window -> BorderTop,
			ww, wh,0xc0);

		FreeBitMap(bitmap);
	}
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
		(UBYTE *) ( (ULONG) output_ri.Memory + (y0*output_ri.BytesPerRow)+(x0*amiga_image_bpp) ), output_ri.BytesPerRow );
}

static void draw_alpha_rgb24 (int x0,int y0, int w,int h, unsigned char* src, unsigned char * srca, int stride)
{
	vo_draw_alpha_rgb24(w,h,src,srca,
		stride,
		(UBYTE *) ( (ULONG) output_ri.Memory + (y0*output_ri.BytesPerRow)+(x0*amiga_image_bpp) ), output_ri.BytesPerRow );
}

static void draw_alpha_rgb16 (int x0,int y0, int w,int h, unsigned char* src, unsigned char * srca, int stride)
{
	vo_draw_alpha_rgb16(w,h,src,srca,
		stride,
		(UBYTE *) ( (ULONG) output_ri.Memory + (y0*output_ri.BytesPerRow)+(x0*amiga_image_bpp) ), output_ri.BytesPerRow );
}



static struct gfx_command commands[] = {

	{	"help",'s',				NULL,	"HELLP/S", gfx_help},
	{	"NOBORDER",	's',		NULL,	"NOBORDER/S", fix_border },
	{	"TINYBORDER",	's',		NULL,	"TINYBORDER/S", fix_border },
	{	"ALWAYSBORDER",	's',	NULL,	"ALWAYSBORDER/S", fix_border },
	{	"DISAPBORDER",	's',	NULL,	"DISAPBORDER/S", fix_border },
	
	{	"16",	's',				NULL,	"16/S (only for comp)", fix_rgb_modes },
	{	"24",	's',				NULL,	"24/S (only for comp)", fix_rgb_modes },
	{	"32",	's',				NULL,	"32/S (only for comp)", fix_rgb_modes },
	
	{	"NOVSYNC",'s',		(ULONG *) &gfx_novsync,	"NOVSYNC/S", NULL },
	{	"MODEID=",'h',		(ULONG *) &WantedModeID,	"MODEID=ScreenModeID/N  (some video outputs might ignore this one)", NULL },
	{	"PUBSCREEN=",'k',	(ULONG *) &gfx_screenname,	"PUBSCREEN=ScreenName/K", NULL },
	{	"MONITOR=",	'n',	(ULONG *) &gfx_monitor,	"MONITOR=Monitor nr/N", NULL },

	{NULL,'?',NULL,NULL, NULL}
};


extern struct Library *GraphicsBase;
extern struct Library *LayersBase;

/******************************** PREINIT ******************************************/
static int preinit(const char *arg)
{
	BOOL have_compositing = FALSE;

	if ( ! LIB_IS_AT_LEAST(GraphicsBase, 54,0) )
	{
		mp_msg(MSGT_VO, MSGL_INFO, "VO: [comp] You need graphics.library version 54.0 or newer.\n");	
		return -1;
	}

	mp_msg(MSGT_VO, MSGL_INFO, "VO: [comp] Welcome man !.\n");

	if (!gfx_process_args( (char *) arg, commands))
	{
		mp_msg(MSGT_VO, MSGL_INFO, "VO: [comp] config is wrong\n");	
		return -1;	
	}

	if (gfx_screenname)
	{
		printf("\n*****\nOpen on screen: %s\n*******\n",gfx_screenname);
	}

	if ( ( My_Screen = LockPubScreen ( gfx_screenname ) ) )
	{
		best_screen_aspect_ratio = (float) My_Screen -> Width / (float) My_Screen -> Height;

		if (GetScreenAttr( My_Screen, SA_Compositing, &have_compositing, sizeof(have_compositing)))
		{
			if (have_compositing == FALSE)
			{
				mp_msg(MSGT_VO, MSGL_INFO, "VO: [comp] Sceeen mode does not have compositing.\n");	
				return -1;
			}
		}
		else
		{
			mp_msg(MSGT_VO, MSGL_INFO, "VO: [comp] Warning: GetScreenAttr can't get SA_Compositing value.\n");	
		}

		UnlockPubScreen(NULL, My_Screen);
	}


	benchmark_frame_cnt = 0;
	gettimeofday(&before,&dontcare);		// to get time before

	return 0;
}

extern char * filename;

static ULONG Open_Window()
{		
	// Window
	ULONG ModeID = INVALID_ID;
	BOOL WindowActivate = TRUE;

	My_Window = NULL;

	if (gfx_screenname)
	{
		printf("\n*****\nOpen on screen: %s\n*******\n",gfx_screenname);
	}

	if ( ( My_Screen = LockPubScreen ( gfx_screenname ) ) )
	{
		ModeID = GetVPModeID(&My_Screen->ViewPort);

		printf("screen %f\n", (float) My_Screen -> Width / (float) My_Screen -> Height);

		print_monitor_info();

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
					WA_SizeGadget,      TRUE,
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
					WA_MinWidth, 		(window_width/3) > 100 ? window_width/3 : 100,
					WA_MinHeight,		(window_height/3) > 100 ? window_height/3 : 100,
					WA_MaxWidth, 		My_Screen -> Width,
					WA_MaxHeight,		My_Screen -> Height,
					WA_SimpleRefresh,		TRUE,
					WA_CloseGadget,     TRUE,
					WA_DepthGadget,     TRUE,
					WA_DragBar,         TRUE,
					WA_Borderless,      (gfx_BorderMode == NOBORDER) ? TRUE : FALSE,
					WA_SizeGadget,      TRUE,
					WA_SizeBBottom,	TRUE,
					WA_Activate,        WindowActivate,
					WA_IDCMP,           IDCMP_COMMON,
					WA_Flags,           WFLG_REPORTMOUSE,
					//WA_SkinInfo,        NULL,
					TAG_DONE);
		}

		vo_screenwidth = My_Screen->Width;
		vo_screenheight = My_Screen->Height;

		vo_dwidth = amiga_image_width;
		vo_dheight = amiga_image_height;

		vo_fs = 0;

		UnlockPubScreen(NULL, My_Screen);
		My_Screen= NULL;
	}


		EmptyPointer = AllocVec(16, MEMF_PUBLIC | MEMF_CLEAR);

	if ( !My_Window || !EmptyPointer)
	{
		mp_msg(MSGT_VO, MSGL_ERR, "Unable to open a window\n");

		if ( gfx_screenname )
		{
			free( gfx_screenname);
			 gfx_screenname = NULL;	// open on workbench if no pubscreen found.
		}

		uninit();
		return INVALID_ID;
	}

	offset_x = (gfx_BorderMode == NOBORDER) ? 0 : My_Window->BorderRight;	
	offset_y = (gfx_BorderMode == NOBORDER) ? 0 : My_Window->BorderTop;

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
	struct Screen *Screen;
	struct DimensionInfo buffer_Dimmension;
	ULONG depth;
	ULONG ModeID;
	ULONG max_width,max_height;

	ULONG left = 0, top = 0;
	ULONG out_width = 0;
	ULONG out_height = 0;

	struct DimensionInfo dimi;

	screen_width=amiga_image_width;
	screen_height=amiga_image_height;

	dprintf("%s:%ld\n",__FUNCTION__,__LINE__);

	if(WantedModeID)
	{
		ModeID = WantedModeID;
	}
	else
	{
		ModeID = find_best_screenmode( gfx_monitor, 32, best_screen_aspect_ratio,amiga_image_width, amiga_image_height);
	}

	mp_msg(MSGT_VO, MSGL_INFO, "VO: [comp] Full screen.\n");

	My_Screen = NULL;

	if ( ModeID != INVALID_ID )
	{

		if (GetDisplayInfoData( NULL, &dimi, sizeof(dimi) , DTAG_DIMS, ModeID))
		{
			depth = ( FALSE ) ? 16 : dimi.MaxDepth ;
		}

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
			TAG_DONE);
	}

	 if ( ! My_Screen ) 
	{
		mp_msg(MSGT_VO, MSGL_ERR, "Unable to open the screen ID:%x\n", (int) ModeID);
	  uninit();
	  return INVALID_ID;
	}


	/* Fill the screen with black color */
	RectFillColor(&(My_Screen->RastPort), 0,0, My_Screen->Width, My_Screen->Height, 0x00000000);

	dprintf("%s:%ld\n",__FUNCTION__,__LINE__);


#ifdef PUBLIC_SCREEN
	PubScreenStatus( My_Screen, 0 );
#endif

	screen_width = My_Screen->Width;
	screen_height = My_Screen->Height;

	offset_x = (screen_width - window_width)/2;
	offset_y = (screen_height - window_height)/2;


	/* calculate new video size/aspect */
	aspect_save_screenres(My_Screen->Width,My_Screen->Height);

	out_width = amiga_image_width;
	out_height = amiga_image_height;

   	aspect(&out_width,&out_height,A_ZOOM);


	dprintf("%s:%ld OpenWindowTags()  \n",__FUNCTION__,__LINE__);

	left=(My_Screen->Width-out_width)/2;
	top=(My_Screen->Height-out_height)/2;

	My_Window = OpenWindowTags( NULL,
#ifdef PUBLIC_SCREEN
			WA_PubScreen,  (ULONG) My_Screen,
#else
			WA_CustomScreen,  (ULONG) My_Screen,
#endif
			WA_PubScreen,       (ULONG) My_Screen,
			WA_Top,		top,
			WA_Left,		left,
			WA_Height,		out_height,
			WA_Width,		out_width,
			WA_SimpleRefresh,   TRUE,
			WA_CloseGadget,     FALSE,
			WA_DragBar,         TRUE,
			WA_Borderless,      TRUE,
			WA_Backdrop,        TRUE,
			WA_Activate,        TRUE,
			WA_IDCMP,           IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_EXTENDEDMOUSE | IDCMP_REFRESHWINDOW | IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW,
			WA_Flags,           WFLG_REPORTMOUSE,
		TAG_DONE);

	if ( ! My_Window) 
	{
		mp_msg(MSGT_VO, MSGL_ERR, "Unable to open a window\n");
		uninit();
		return INVALID_ID;
	}

    SetWindowAttrs(My_Window,
                               WA_Left,left,
                               WA_Top,top,
                               WA_Width,out_width,
                               WA_Height,out_height,
                               TAG_DONE);

	RectFillColor( My_Window->RPort, 0,0, screen_width, screen_height, 0x00000000);

	Printf("Screen w %ld h %ld\n",My_Screen->Width,My_Screen->Height);

	vo_screenwidth = My_Screen->Width;
	vo_screenheight = My_Screen->Height;

	vo_dwidth = amiga_image_width;
	vo_dheight = amiga_image_height;
	
	vo_fs = 1;

	gfx_ControlBlanker(My_Screen, FALSE);

	return ModeID;
} 

static int PrepareBuffer(uint32_t in_format, uint32_t out_format)
{
	swsContext= sws_getContextFromCmdLine(amiga_image_width, amiga_image_height, in_format, amiga_image_width, amiga_image_height, out_format );

	if (!swsContext)
	{
		uninit();
		return -1;
	}

	return 0;
}


/******************************** CONFIG ******************************************/
static int config(uint32_t width, uint32_t height, uint32_t d_width,
		     uint32_t d_height, uint32_t flags, char *title,
		     uint32_t in_format)
{
	ULONG ModeID = INVALID_ID;
	uint32 BMLock;
	uint32 fmt;

	if (My_Window) return 0;

	is_fullscreen = flags & VOFLAG_FULLSCREEN;
	is_paused = FALSE;

	amiga_image_format = in_format; 

	printf("vo_directrendering %d\n",vo_directrendering);

	switch (gfx_common_rgb_format )
	{
		case PIXF_A8R8G8B8:	amiga_image_bpp = 4;	break;
		case PIXF_R8G8B8: amiga_image_bpp = 3;	break;
		case PIXF_R5G6B5: amiga_image_bpp = 2; 	break;
		default: 
			amiga_image_bpp = 4;
			gfx_common_rgb_format = PIXF_A8R8G8B8;
	}


	org_amiga_image_width = width;
	org_amiga_image_height =height;

	amiga_image_width = width & -16;
	amiga_image_height = height ;

	amiga_aspect_ratio = (float) d_width /  (float) d_height;

	window_width = d_width ;
	window_height = d_height;

	while ((window_width<300)||(window_height<300))
	{
		window_width *= 2;
		window_height *= 2;
	}

	ModeID = INVALID_ID;

	MutexObtain( window_mx );
	
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

		if (!My_Window)
		{
			MutexRelease(window_mx);
			uninit();
			return -1;
		}

		if (the_bitmap)
		{
			Printf("%s:%ld -- Free BitMap here\n",__FUNCTION__,__LINE__);
			FreeBitMap(the_bitmap);
		}

		the_bitmap = AllocBitMap( amiga_image_width , amiga_image_height,  32 , BMF_DISPLAYABLE, My_Window ->RPort -> BitMap);

		alloc_output_buffer( org_amiga_image_width, org_amiga_image_height );

		if (!output_ri.Memory)
		{
			Printf("******* buffer not allocated :-(, some info bpp=%ld \n ", amiga_image_bpp);
		}

		rp = My_Window->RPort;

		UserMsg = My_Window->UserPort;

	MutexRelease(window_mx);

	switch (gfx_common_rgb_format )
	{
		case PIXF_A8R8G8B8:	
			draw_alpha_func = draw_alpha_rgb32;
			fmt = IMGFMT_BGR32; 
			break;
		case PIXF_R8G8B8:
			draw_alpha_func = draw_alpha_rgb24;
			fmt = IMGFMT_RGB24; 
			break;
		case PIXF_R5G6B5: 
			draw_alpha_func = draw_alpha_rgb16;
			fmt = IMGFMT_BGR16; 
			break;
		default: 	fmt = IMGFMT_BGR32; break;
	}

 	if (PrepareBuffer(in_format, fmt ) < 0) 
	{
		uninit();
		return -1;
	}



	gfx_Start(My_Window);

#ifdef CONFIG_GUI
    if (use_gui)
		guiGetEvent(guiSetWindowPtr, (char *) My_Window);
#endif

	return 0; // -> Ok
}

int pub_frame = 0;

/******************************** DRAW_SLICE ******************************************/
static int draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{

	uint8_t *dst[3];
  	int dstStride[3];



	MutexObtain( window_mx );

	if (( amiga_image_width != w )&&(x==0) )
	{
		printf("%d,%d,%d,%d,%d\n",amiga_image_width,w,h,x,y);

		if (output_ri.Memory) { FreeVec(output_ri.Memory); output_ri.Memory = NULL; }
		alloc_output_buffer( w, h );
	}


	dstStride[0] = org_amiga_image_width*amiga_image_bpp;
	dstStride[1] = 0;
	dstStride[2] = 0;

	dst[0] = (uint8_t *) ( (ULONG) output_ri.Memory +x*amiga_image_bpp) ;
	dst[1] = NULL;
	dst[2] = NULL; 

	sws_scale(swsContext, 
			image,
			stride,
			y,
			h,
			dst, 
			dstStride);

	MutexRelease(window_mx);

	return 0;
}
/******************************** DRAW_OSD ******************************************/

static void draw_osd(void)
{
	vo_draw_text(amiga_image_width,amiga_image_height,draw_alpha_func);
}

/******************************** FLIP_PAGE ******************************************/
static void flip_page(void)
{
	struct RastPort rp;
	int ww,wh;

	benchmark_frame_cnt ++;

	if ((My_Window)&&(the_bitmap))
	{
		ww = My_Window->Width - My_Window->BorderLeft - My_Window->BorderRight;
		wh = My_Window->Height - My_Window->BorderTop - My_Window->BorderBottom;
	
		if ((ww==amiga_image_width)&&(wh==amiga_image_height))	// no scaling so we dump it into the window.
		{
//			if (gfx_novsync==FALSE) WaitTOF();
			WritePixelArray( output_ri.Memory, 0, 0, output_ri.BytesPerRow, PIXF_A8R8G8B8, 
					My_Window -> RPort, My_Window->BorderLeft,My_Window->BorderTop, amiga_image_width,amiga_image_height);
		}
		else
		{
			InitRastPort(&rp);
			rp.BitMap = the_bitmap;

			WritePixelArray( output_ri.Memory, 0, 0, output_ri.BytesPerRow, PIXF_A8R8G8B8, 
					&rp, 0,0, amiga_image_width,amiga_image_height);

//			if (gfx_novsync==FALSE) WaitTOF();
			if (is_fullscreen)	// do it fast
			{
				draw_comp( the_bitmap,My_Window, amiga_image_width,amiga_image_height);
			}
			else				// try not to trash graphics.
			{
				draw_inside_window();
			}
		}
	}
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

	Delay(1);

	MutexObtain( window_mx );

	// if screen : close Window thn screen
	if (My_Screen) 
	{
		CloseWindow(My_Window);
		My_Window=NULL;

#ifdef CONFIG_GUI
		if(use_gui)
		{
			if(CloseScreen(My_Screen))
			{
				mygui->screen = NULL;
			}
		}
		else
		{
			CloseScreen(My_Screen);
		}
#else
		CloseScreen(My_Screen);
#endif
		My_Screen = NULL;
	}

	if (My_Window) 
	{
		gfx_StopWindow(My_Window);
		if (rp) InstallLayerHook(rp->Layer, NULL);
		CloseWindow(My_Window);
		My_Window=NULL;
	}

	rp = NULL;

	if (the_bitmap) { FreeBitMap(the_bitmap); 	the_bitmap = NULL; }
	if (output_ri.Memory)
	{
		dprintf("%s:%ld -- Free image buffer here\n",__FUNCTION__,__LINE__);
		FreeVec(output_ri.Memory);	output_ri.Memory = NULL;
	}

	if (inner_bitmap) { FreeBitMap(inner_bitmap);  inner_bitmap = NULL; }

	MutexRelease(window_mx);

	dprintf("%s:%ld\n",__FUNCTION__,__LINE__);
}

/******************************** UNINIT    ******************************************/
static void uninit(void)
{
	unsigned long long int microsec;	

	if (benchmark_frame_cnt>0)
	{
		gettimeofday(&after,&dontcare);		// to get time after
		microsec = (after.tv_usec - before.tv_usec) +(1000000 * (after.tv_sec - before.tv_sec));
		printf("Internal COMP FPS %0.00f\n",(float)  benchmark_frame_cnt / (float) (microsec / 1000 / 1000) );
	}
	benchmark_frame_cnt = 0;

dprintf("%s:%ld - rp is 0x%08lx \n",__FUNCTION__,__LINE__,rp);
	FreeGfx();
dprintf("%s:%ld - rp is 0x%08lx \n",__FUNCTION__,__LINE__,rp);
	if (EmptyPointer)
	{
	  FreeVec(EmptyPointer);
	  EmptyPointer=NULL;
	}
dprintf("%s:%ld - rp is 0x%08lx \n",__FUNCTION__,__LINE__,rp);
	gfx_ReleaseArg();
dprintf("%s:%ld - rp is 0x%08lx \n",__FUNCTION__,__LINE__,rp);
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
			if ( config(org_amiga_image_width, org_amiga_image_height, window_width, window_height, is_fullscreen, NULL, amiga_image_format) < 0) return VO_FALSE;
			return VO_TRUE;

		case VOCTRL_PAUSE:

			gfx_Stop(My_Window);
			if (rp) InstallLayerHook(rp->Layer, &BackFill_Hook);

			gfx_ControlBlanker(My_Screen, TRUE);

			is_paused = is_paused ? FALSE : TRUE;

			BackFill_Func( My_Window -> RPort, NULL );
			return VO_TRUE;					

		case VOCTRL_RESUME:

			gfx_Start(My_Window);

			if (rp) InstallLayerHook(rp->Layer, NULL );

			gfx_ControlBlanker(My_Screen, FALSE);

			is_paused = is_paused ? FALSE : TRUE;

			BackFill_Func( My_Window -> RPort, NULL );

			return VO_TRUE;

		case VOCTRL_QUERY_FORMAT:
			return query_format(*(ULONG *)data);

//		case VOCTRL_GET_IMAGE:
//			return get_image(data);
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

