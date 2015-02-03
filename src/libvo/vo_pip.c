/*
 *  vo_pip.c
 *  VO module for MPlayer AmigaOS
 *  Writen by Jörg strohmayer
 */

#ifdef __ALTIVEC__
#warning No AltiVec code here :-(   Reimplement the gfxcopy*() functions
#endif

#include "cgx_common.h"

#define NO_DRAW_FRAME
#define NO_DRAW_SLICE

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/unistd.h>
#include <sys/param.h>

#include "config.h"
#include "aspect.h"
#include "input/input.h"
#include "input/mouse.h"
#include "libmpcodecs/vf_scale.h"
#include "mplayer.h"
#include "mp_fifo.h"
#include "mp_msg.h"
#include "osdep/keycodes.h"
#include "sub/sub.h"
#include "sub/eosd.h"
#include "subopt-helper.h"
#include "video_out.h"
#include "video_out_internal.h"

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/Picasso96API.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <graphics/blitattr.h>
#include <graphics/composite.h>
#include <graphics/gfxbase.h>
#include <graphics/layers.h>
#include <graphics/rpattr.h>
#include <graphics/text.h>
#include <libraries/keymap.h>
#include <graphics/gfx.h>

//--becnhmark--
static struct timezone dontcare = { 0,0 };
static struct timeval before, after;
static ULONG benchmark_frame_cnt = 0;
extern int benchmark;
//------------------

static vo_info_t info =
{
    "Picasso96 PIP video output",
    "pip",
    "Joerg Strohmayer",
    ""
};

LIBVO_EXTERN(pip)

static uint32_t   win_left;             
static uint32_t   win_top;              
static BOOL FirstTime = TRUE;

static struct Window *My_Window = NULL;

static uint32 BorderMode = ALWAYSBORDER;

static uint32 image_width;
static uint32 image_height;
static uint32 image_format;
static uint32 out_format;
static uint32 window_width;
static uint32 window_height;
static char * window_title;
static uint32 pip_format;

extern float	amiga_aspect_ratio;
static float	best_screen_aspect_ratio;

#ifdef USE_RGB
static int16  yuv2rgb[5][255];
#endif
static uint32 ModeID;

/***************************************************/

static char *GetWindowTitle(void)
{
   if (window_title) free(window_title);

   if (filename)
   {
      window_title = (char *)malloc(strlen("MPlayer - ") + strlen(filename) + 1);
      strcpy(window_title, "MPlayer - ");
      strcat(window_title, filename);
   } else {
      window_title = strdup("MPlayer");
   }

   return window_title;
}

/******************************** DRAW ALPHA ******************************************/
static void draw_alpha(int x, int y, int w, int h, UNUSED unsigned char* src, unsigned char *srca, int stride)
{
   static uint32 clut[256];

   if (0 == clut[0])
   {
      int i;

      clut[0] = 0xFF110011;

      for (i = 1 ; i < 256 ; i++)
      {
         clut[i] = (256-i)<<24 | (256-i) << 16 | (256-i) << 8 | (256-i);
      }
   }

      BltBitMapTags(BLITA_Source, srca,
                               BLITA_SrcBytesPerRow, stride,
                               BLITA_SrcType, BLITT_CHUNKY,
                               BLITA_Dest, My_Window->RPort,
                               BLITA_DestType, BLITT_RASTPORT,
                               BLITA_DestX, My_Window->BorderLeft + x,
                               BLITA_DestY, My_Window->BorderTop + y,
                               BLITA_Width, w,
                               BLITA_Height, h,
                               BLITA_CLUT, clut,
                               TAG_DONE);
}

/******************************** DRAW_OSD ******************************************/
static void draw_osd(void)
{
   vo_draw_text_ext(My_Window->GZZWidth, My_Window->GZZHeight, 0, 0, 0, 0, image_width, image_height, draw_alpha);
}

/**********************/
static void Close_Window(void)
{
   if (My_Window)
   {
      ClearMenuStrip(My_Window);
         p96PIP_Close(My_Window);
      My_Window = NULL;
   }
}

   struct Screen * My_Screen = NULL;

static uint32 Open_Window(uint32 hidden)
{
   ModeID = INVALID_ID;

   if ((My_Screen = LockPubScreen(NULL)))
   {
      ModeID = GetVPModeID(&My_Screen->ViewPort);


		if (FirstTime)
		{
			gfx_center_window(My_Screen, window_width, window_height, &win_left, &win_top);
		}

	static struct IBox zoombox = {0, 0, 9999, 9999};

	zoombox.Width  = My_Screen->Width;
	zoombox.Height = My_Screen->Height;
	{
                     int32 sourcewidth = image_width;
                     int32 sourceheight = RGBFB_YUV410P == pip_format ? image_height & ~3 : RGBFB_YUV420P == pip_format ? image_height & ~1 : /*RGBFB_YUV422CGX*/ image_height;

                     My_Window = p96PIP_OpenTags(
                        P96PIP_SourceFormat, pip_format,
                        P96PIP_SourceWidth, sourcewidth,
                        P96PIP_SourceHeight, sourceheight,
                        P96PIP_NumberOfBuffers, 3,

                        WA_CustomScreen,    (uint32) My_Screen,
                        ALWAYSBORDER == BorderMode ? WA_Title : WA_WindowName, GetWindowTitle(),
                        WA_ScreenTitle,     (uint32) "MPlayer",
                        WA_Left,            win_left,
                        WA_Top,            win_top,
                        WA_NewLookMenus,    TRUE,
                        WA_CloseGadget,     ALWAYSBORDER == BorderMode ? TRUE : FALSE,
                        WA_DepthGadget,     ALWAYSBORDER == BorderMode ? TRUE : FALSE,
                        WA_DragBar,         ALWAYSBORDER == BorderMode ? TRUE : FALSE,
                        WA_Borderless,      NOBORDER     == BorderMode ? TRUE : FALSE,
                        WA_SizeGadget,      ALWAYSBORDER == BorderMode ? TRUE : FALSE,
                        ALWAYSBORDER == BorderMode ? WA_SizeBBottom : TAG_IGNORE, TRUE,
                        WA_Activate,        TRUE,
                        WA_StayTop,         (vo_ontop==1) ? TRUE : FALSE,
                        WA_IDCMP,           IDCMP_COMMON,
                        WA_InnerWidth,      window_width,
                        WA_InnerHeight,     window_height,
                        WA_MinWidth,        160,
                        WA_MinHeight,       100,
                        WA_MaxWidth,        ~0,
                        WA_MaxHeight,       ~0,
                        ALWAYSBORDER == BorderMode ? WA_Zoom : TAG_IGNORE, &zoombox,
                        WA_Hidden,          hidden,
                     TAG_DONE);

	}
	UnlockPubScreen(NULL, My_Screen);
   }

   if (!My_Window)
   {
      mp_msg(MSGT_VO, MSGL_FATAL, "[pip] Unable to open a window\n");
      return INVALID_ID;
   }

   if (ModeID != INVALID_ID)
   {
      ScreenToFront(My_Screen);
   }

   return ModeID;
}

/******************************** PREINIT ******************************************/
static int opt_border, opt_mode, opt_help, opt_question;
static const opt_t subopts[] =
{
   { "border",   OPT_ARG_INT,  &opt_border,   NULL },
   { "mode",     OPT_ARG_INT,  &opt_mode,     NULL },
   { "help",     OPT_ARG_BOOL, &opt_help,     NULL },
   { "?",        OPT_ARG_BOOL, &opt_question, NULL },
   { NULL, 0, NULL, NULL }
};

#define NUM_MODES 3

static int preinit(const char *arg)
{
   struct Screen *screen;
   int i;

   doubleclick_time = 0;

   mp_msg(MSGT_VO, MSGL_INFO, "VO: [pip]\n");

   opt_border   = 2;
   opt_mode     = 0;

   if (subopt_parse(arg, subopts) != 0 || opt_help || opt_question)
   {
      mp_msg(MSGT_VO, MSGL_FATAL,
             "-vo pip command line help:\n"
             "Example: mplayer -vo pip:border=0:mode=1\n"
             "\nOptions:\n"
             "  border\n"
             "    0: Broderless window\n"
             "    1: Window with small borders\n"
             "    2: Normal window (default)\n"
             "  mode\n"
             "    0: YUV410 (default)\n"
             "    1: YUV420\n"
             "    2: YUV422\n"
             "\n");
      return VO_ERROR;
   }

   switch (opt_border)
   {
      case 0: BorderMode = NOBORDER;     break;
      case 1: BorderMode = TINYBORDER;   break;
      case 2: BorderMode = ALWAYSBORDER; break;
      default: mp_msg(MSGT_VO, MSGL_FATAL, "[pip] Unsupported 'border=%d' option\n", opt_border);
      return VO_ERROR;
   }

   switch (opt_mode)
   {
      case 0: pip_format = RGBFB_YUV410P;   break;
      case 1: pip_format = RGBFB_YUV420P;   break;
      case 2: pip_format = RGBFB_YUV422CGX; break;
      default: mp_msg(MSGT_VO, MSGL_FATAL, "[pip] Unsupported 'mode=%d' option\n", opt_mode);
      return VO_ERROR;
   }

	benchmark_frame_cnt = 0;
	gettimeofday(&before,&dontcare);		// to get time before

	if ( ( My_Screen = LockPubScreen ( NULL) ) )
	{
		best_screen_aspect_ratio = (float) My_Screen -> Width / (float) My_Screen -> Height;
		UnlockPubScreen(NULL, My_Screen);
	}

   return 0;
}

/******************************** CONFIG ******************************************/
static int config(uint32_t width, uint32_t height, uint32_t d_width,
             uint32_t d_height, uint32_t flags, UNUSED char *title,
             uint32_t format)
{
   if (My_Window) Close_Window();

   image_format = format;

   vo_fs = flags & VOFLAG_FULLSCREEN;

   image_width = width;
   image_height = height;
   if (image_width > 1520)
   {
      pip_format = 0;
   }

   window_width = d_width;
   window_height = d_height;

   amiga_aspect_ratio = (float) d_width /  (float) d_height;
   
   vo_fs = VO_FALSE;

   window_width = d_width;
   window_height = d_height;

   ModeID = Open_Window(FALSE);
   if (INVALID_ID == ModeID)
   {
      return VO_ERROR;
   }

   out_format = 0 == pip_format ? IMGFMT_RGB16 : IMGFMT_YV12;
   return 0;
}

/**********************/
static void gfxcopy(uint8 *s, uint8 *d, uint32 l)
{
   if (l >= 4)
   {
      uint32 i;
      if (l >= 8)
      {
         if (((uint32)s & 3) || ((uint32)d & 7))
         {
            uint32 *sL = (uint32 *)s, *dL = (uint32 *)d, *asL = (uint32 *)(((uint32)s + 31) & ~31);
            asm volatile ("dcbt 0,%0" : : "r" (&asL[0]));
            for (i=l/(8*sizeof(uint32));i;i--)
            {
               uint32 t1, t2;
               if (i > 1) asm volatile ("dcbt 0,%0" : : "r" (&asL[8]));
               t1=sL[0]; t2=sL[1]; dL[0]=t1; dL[1]=t2; t1=sL[2]; t2=sL[3]; dL[2]=t1; dL[3]=t2;
               t1=sL[4]; t2=sL[5]; dL[4]=t1; dL[5]=t2; t1=sL[6]; t2=sL[7]; dL[6]=t1; dL[7]=t2;
               asL += 8; sL += 8; dL += 8;
            }
            l &= 8*sizeof(uint32)-1; s = (uint8 *)sL; d = (uint8 *)dL;
         } else {
            double *s64 = (double *)s, *d64 = (double *)d, *as64 = (double *)(((uint32)s + 31) & ~31);
            asm volatile ("dcbt 0,%0" : : "r" (&as64[0]));
            for (i=l/(4*sizeof(double));i;i--)
            {
               double t1, t2, t3, t4;
               if (i > 1) asm volatile ("dcbt 0,%0" : : "r" (&as64[4]));
               t1 = s64[0]; t2 = s64[1]; t3 = s64[2]; t4 = s64[3];
               d64[0] = t1; d64[1] = t2; d64[2] = t3; d64[3] = t4;
               as64 += 4; s64 += 4; d64 += 4;
            }
            l &= 4*sizeof(double)-1;
            for (i=l/sizeof(double);i;i--) *d64++ = *s64++;
            l &= sizeof(double)-1; s = (uint8 *)s64; d = (uint8 *)d64;
         }
      }
      if (l >= 4)
      {
         uint32 *sL = (uint32 *)s, *dL = (uint32 *)d;
         for (i=l/sizeof(uint32);i;i--) *dL++ = *sL++;
         l &= sizeof(uint32)-1; s = (uint8 *)sL; d = (uint8 *)dL;
      }
   }
   while (l--) *d++ = *s++;
}


static void gfxcopy_420P_410P(uint8 *s, uint8 *d, uint32 l)
{
   #define pL(a,b) (a&0xFF000000)|(a<<8&0xFF0000)|(b>>16&0xFF00)|(b>>8&0xFF)
   #define pQ(a,b) (((uint64)pL(a>>32,a))<<32)|pL(b>>32,b)
   if (l >= 4)
   {
      uint32 i;
      if (l >= 8)
      {
         if (((uint32)s & 3) || ((uint32)d & 7))
         {
            uint32 *sL = (uint32 *)s, *dL = (uint32 *)d, *asL = (uint32 *)(((uint32)s + 31) & ~31);
            asm volatile ("dcbt 0,%0" : : "r" (&asL[0]));
            for (i=l/(4*sizeof(uint32));i;i--)
            {
               uint32 t1, t2;
               if (i > 1) asm volatile ("dcbt 0,%0" : : "r" (&asL[8]));
               t1=sL[0]; t2=sL[1]; dL[0]=pL(t1, t2);
               t1=sL[2]; t2=sL[3]; dL[1]=pL(t1, t2);
               t1=sL[4]; t2=sL[5]; dL[2]=pL(t1, t2);
               t1=sL[6]; t2=sL[7]; dL[3]=pL(t1, t2);
               asL += 8; sL += 8; dL += 4;
            }
            l &= 4*sizeof(uint32)-1; s = (uint8 *)sL; d = (uint8 *)dL;
         } else {
            uint64 *s64 = (uint64 *)s;
            volatile double *d64 = (volatile double *)d, *as64 = (double *)(((uint32)s + 31) & ~31);
            asm volatile ("dcbt 0,%0" : : "r" (&as64[0]));
            for (i=l/(4*sizeof(double));i;i--)
            {
               uint64 t1, t2, t3, t4, d0, d1, d2, d3;
               if (i > 1) asm volatile ("dcbt 0,%0" : : "r" (&as64[4]));
               t1 = s64[0]; t2 = s64[1]; d0 = pQ(t1, t2);
               t3 = s64[2]; t4 = s64[3]; d1 = pQ(t3, t4);
               if (i > 2) asm volatile ("dcbt 0,%0" : : "r" (&as64[8]));
               t1 = s64[4]; t2 = s64[5]; d2 = pQ(t1, t2);
               t3 = s64[6]; t4 = s64[7]; d3 = pQ(t3, t4);
               d64[0] = *(volatile double *)&d0; d64[1] = *(volatile double *)&d1;
               d64[2] = *(volatile double *)&d2; d64[3] = *(volatile double *)&d3;
               as64 += 8; s64 += 8; d64 += 4;
            }
            l &= 4*sizeof(double)-1;
            for (i=l/sizeof(double);i;i--)
            {
               uint64 t1, t2; volatile uint64 d0;
               t1 = s64[0]; t2 = s64[1]; d0 = pQ(t1, t2);
               d64[0] = *(volatile double *)&d0;
               s64 += 2; d64 += 1;
            }
            l &= sizeof(double)-1;
            s = (uint8 *)s64; d = (uint8 *)d64;
         }
      }
      if (l >= 4)
      {
         uint32 *sL = (uint32 *)s, *dL = (uint32 *)d;
         for (i=l/sizeof(uint32);i;i--)
         {
            uint32 t1 = sL[0]; uint32 t2 = sL[1];
            dL[0] = pL(t1, t2); sL += 2; dL += 1;
         }
         l &= sizeof(uint32)-1; s = (uint8 *)sL; d = (uint8 *)dL;
      }
   }
   while (l--)
   {
      *d++ = *s;
      s += 2;
   }
}


static void gfxcopy_420P_422CGX(uint8 *sY0, uint8 *sY1, uint8 *sU, uint8 *sV, uint8 *d1, uint8 *d2, uint32 l)
{
   #define pL0(Y,U,V) (Y    &0xFF000000)|(U>> 8&0xFF0000)|(Y>>8&0xFF00)|(V>>24&0xFF)
   #define pL1(Y,U,V) (Y<<16&0xFF000000)|(U    &0xFF0000)|(Y<<8&0xFF00)|(V>>16&0xFF)
   #define pL2(Y,U,V) (Y    &0xFF000000)|(U<< 8&0xFF0000)|(Y>>8&0xFF00)|(V>> 8&0xFF)
   #define pL3(Y,U,V) (Y<<16&0xFF000000)|(U<<16&0xFF0000)|(Y<<8&0xFF00)|(V    &0xFF)
   #define pQ0(Y,U,V) ((uint64)(pL0(Y>>32,U,V))<<32)|pL1(Y>>32,U,V)
   #define pQ1(Y,U,V) ((uint64)(pL2(Y    ,U,V))<<32)|pL3(Y    ,U,V)
   if (l >= 4)
   {
      uint32 i;
      if (((uint32)sY0 & 7) || ((uint32)sY1 & 7) || ((uint32)d1 & 7) || ((uint32)d2 & 7))
      {
         uint32 *d1L  = (uint32 *)d1,  *d2L = (uint32 *)d2;
         uint32 *Y0L  = (uint32 *)sY0, *Y1L = (uint32 *)sY1;
         uint32 *UL   = (uint32 *)sU,  *VL  = (uint32 *)sV;
         uint32 *aY0L = (uint32 *)(((uint32)sY0 + 31) & ~31);
         uint32 *aY1L = (uint32 *)(((uint32)sY1 + 31) & ~31);
         uint32 *aUL  = (uint32 *)(((uint32)sU  + 31) & ~31);
         uint32 *aVL  = (uint32 *)(((uint32)sV  + 31) & ~31);
         asm volatile ("dcbt 0,%0" : : "r" (&aY0L[0]));
         asm volatile ("dcbt 0,%0" : : "r" (&aY1L[0]));
         asm volatile ("dcbt 0,%0" : : "r" (&aUL[0]));
         asm volatile ("dcbt 0,%0" : : "r" (&aVL[0]));
         for (i=l/4;i;i--)
         {
            uint32 Y0, U, Y1, V;
            if (i > 1)
            {
               asm volatile ("dcbt 0,%0" : : "r" (&aY0L[8]));
               asm volatile ("dcbt 0,%0" : : "r" (&aY1L[8]));
               asm volatile ("dcbt 0,%0" : : "r" (&aUL[8]));
               asm volatile ("dcbt 0,%0" : : "r" (&aVL[8]));
            }
            Y0 = Y0L[0]; Y1 = Y1L[0]; U = UL[0]; V = VL[0];
            d1L[ 0]=pL0(Y0,U,V); d1L[ 1]=pL1(Y0,U,V); d2L[ 0]=pL0(Y1,U,V); d2L[ 1]=pL1(Y1,U,V);
            Y0 = Y0L[1]; Y1 = Y1L[1];
            d1L[ 2]=pL2(Y0,U,V); d1L[ 3]=pL3(Y0,U,V); d2L[ 2]=pL2(Y1,U,V); d2L[ 3]=pL3(Y1,U,V);
            Y0 = Y0L[2]; Y1 = Y1L[2]; U = UL[1]; V = VL[1];
            d1L[ 4]=pL0(Y0,U,V); d1L[ 5]=pL1(Y0,U,V); d2L[ 4]=pL0(Y1,U,V); d2L[ 5]=pL1(Y1,U,V);
            Y0 = Y0L[3]; Y1 = Y1L[3];
            d1L[ 6]=pL2(Y0,U,V); d1L[ 7]=pL3(Y0,U,V); d2L[ 6]=pL2(Y1,U,V); d2L[ 7]=pL3(Y1,U,V);
            Y0 = Y0L[4]; Y1 = Y1L[4]; U = UL[2]; V = VL[2];
            d1L[ 8]=pL0(Y0,U,V); d1L[ 9]=pL1(Y0,U,V); d2L[ 8]=pL0(Y1,U,V); d2L[ 9]=pL1(Y1,U,V);
            Y0 = Y0L[5]; Y1 = Y1L[5];
            d1L[10]=pL2(Y0,U,V); d1L[11]=pL3(Y0,U,V); d2L[10]=pL2(Y1,U,V); d2L[11]=pL3(Y1,U,V);
            Y0 = Y0L[6]; Y1 = Y1L[6]; U = UL[3]; V = VL[3];
            d1L[12]=pL0(Y0,U,V); d1L[13]=pL1(Y0,U,V); d2L[12]=pL0(Y1,U,V); d2L[13]=pL1(Y1,U,V);
            Y0 = Y0L[7]; Y1 = Y1L[7];
            d1L[14]=pL2(Y0,U,V); d1L[15]=pL3(Y0,U,V); d2L[14]=pL2(Y1,U,V); d2L[15]=pL3(Y1,U,V);
            d1L += 16; d2L  += 16;
            Y0L += 8;  aY0L += 8; Y1L += 8; aY1L += 8;
            UL  += 4;  aUL  += 4; VL  += 4; aVL  += 4;
         }
         l &= 4-1;
         d1  = (uint8 *)d1L; d2  = (uint8 *)d2L;
         sY0 = (uint8 *)Y0L; sY1 = (uint8 *)Y1L;
         sU  = (uint8 *)UL;  sV  = (uint8 *)VL;
      } else {
         volatile double *d1Q  = (volatile double *)d1,  *d2Q = (volatile double *)d2;
         uint64 *Y0Q  = (uint64 *)sY0, *Y1Q = (uint64 *)sY1;
         uint32 *UL   = (uint32 *)sU,   *VL = (uint32 *)sV;
         uint64 *aY0Q = (uint64 *)(((uint32)sY0 + 31) & ~31);
         uint64 *aY1Q = (uint64 *)(((uint32)sY1 + 31) & ~31);
         uint32 *aUL  = (uint32 *)(((uint32)sU  + 31) & ~31);
         uint32 *aVL  = (uint32 *)(((uint32)sV  + 31) & ~31);
         asm volatile ("dcbt 0,%0" : : "r" (&aY0Q[0]));
         asm volatile ("dcbt 0,%0" : : "r" (&aY1Q[0]));
         asm volatile ("dcbt 0,%0" : : "r" (&aUL[0]));
         asm volatile ("dcbt 0,%0" : : "r" (&aVL[0]));
         for (i=l/4;i;i--)
         {
            uint64 Y0, U, Y1, V;
            volatile uint64 d10, d11, d20, d21;
            if (i > 1)
            {
               asm volatile ("dcbt 0,%0" : : "r" (&aY0Q[4]));
               asm volatile ("dcbt 0,%0" : : "r" (&aY1Q[4]));
               asm volatile ("dcbt 0,%0" : : "r" (&aUL[8]));
               asm volatile ("dcbt 0,%0" : : "r" (&aVL[8]));
            }
            Y0  = Y0Q[0]; Y1 = Y1Q[0]; U = UL[0]; V = VL[0];
            d10 = pQ0(Y0,U,V); d11 = pQ1(Y0,U,V); d20 = pQ0(Y1,U,V); d21 = pQ1(Y1,U,V);
            d1Q[0] = *(volatile double *)&d10; d1Q[1] = *(volatile double *)&d11;
            d2Q[0] = *(volatile double *)&d20; d2Q[1] = *(volatile double *)&d21;
            Y0  = Y0Q[1]; Y1 = Y1Q[1]; U = UL[1]; V = VL[1];
            d10 = pQ0(Y0,U,V); d11 = pQ1(Y0,U,V); d20 = pQ0(Y1,U,V); d21 = pQ1(Y1,U,V);
            d1Q[2] = *(volatile double *)&d10; d1Q[3] = *(volatile double *)&d11;
            d2Q[2] = *(volatile double *)&d20; d2Q[3] = *(volatile double *)&d21;
            Y0  = Y0Q[2]; Y1 = Y1Q[2]; U = UL[2]; V = VL[2];
            d10 = pQ0(Y0,U,V); d11 = pQ1(Y0,U,V); d20 = pQ0(Y1,U,V); d21 = pQ1(Y1,U,V);
            d1Q[4] = *(volatile double *)&d10; d1Q[5] = *(volatile double *)&d11;
            d2Q[4] = *(volatile double *)&d20; d2Q[5] = *(volatile double *)&d21;
            Y0  = Y0Q[3]; Y1 = Y1Q[3]; U = UL[3]; V = VL[3];
            d10 = pQ0(Y0,U,V); d11 = pQ1(Y0,U,V); d20 = pQ0(Y1,U,V); d21 = pQ1(Y1,U,V);
            d1Q[6] = *(volatile double *)&d10; d1Q[7] = *(volatile double *)&d11;
            d2Q[6] = *(volatile double *)&d20; d2Q[7] = *(volatile double *)&d21;
            d1Q += 8; d2Q  += 8; 
            Y0Q += 4; aY0Q += 4; Y1Q += 4; aY1Q += 4; UL += 4; aUL += 4; VL += 4; aVL += 4;
         }
         l &= 4-1;
         d1  = (uint8 *)d1Q; d2  = (uint8 *)d2Q;
         sY0 = (uint8 *)Y0Q; sY1 = (uint8 *)Y1Q;
         sU  = (uint8 *)UL;  sV  = (uint8 *)VL;
      }
   }
   if (l)
   {
      uint32 *d1L = (uint32 *)d1,  *d2L = (uint32 *)d2;
      uint32 *Y0L = (uint32 *)sY0, *Y1L = (uint32 *)sY1;
      uint32 *UL  = (uint32 *)sU,  *VL  = (uint32 *)sV;
      while (l--)
      {
         uint32 Y0, U, Y1, V;
         Y0 = *Y0L++; Y1 = *Y1L++; U = *UL++; V = *VL++;
         *d1L++ = pL0(Y0,U,V); *d1L++ = pL1(Y0,U,V);
         *d2L++ = pL0(Y1,U,V); *d2L++ = pL1(Y1,U,V);
         Y0 = *Y0L++; Y1 = *Y1L++;
         *d1L++ = pL2(Y0,U,V); *d1L++ = pL3(Y0,U,V);
         *d2L++ = pL2(Y1,U,V); *d2L++ = pL3(Y1,U,V);
      }
   }
}



/******************************** FLIP_PAGE ******************************************/
static void flip_page(void)
{
   if (!My_Window)
   {
      mp_msg(MSGT_VO, MSGL_ERR, "[pip] no window\n");
      return;
   }

      uint32 numbuffers = 0, WorkBuf/*, DispBuf*/;

      p96PIP_GetTags(My_Window, P96PIP_NumberOfBuffers, &numbuffers,  P96PIP_WorkBuffer, &WorkBuf, TAG_DONE);

      if (numbuffers > 1)
      {
         uint32 NextWorkBuf = (WorkBuf+1) % numbuffers;

         p96PIP_SetTags(My_Window, P96PIP_VisibleBuffer, WorkBuf, P96PIP_WorkBuffer, NextWorkBuf, TAG_DONE);
      }

	benchmark_frame_cnt++;
}

/******************************** DRAW_IMAGE ******************************************/
static int32 draw_image(mp_image_t *mpi)
{
   struct YUVRenderInfo ri;
   struct BitMap *bm = NULL;
   uint8 *srcY, *srcU, *srcV, *dst, *dstU, *dstV;
   uint8_t **image = mpi->planes;
   int *stride = mpi->stride;
   int w = mpi->width;
   int h = mpi->height;
   int x = mpi->x;
   int y = mpi->y;

   if (!My_Window)
   {
      mp_msg(MSGT_VO, MSGL_ERR, "[pip] no window\n");
      return VO_TRUE; //VO_ERROR;  never return VO_ERROR or the NULL draw_slice() may still get called
   }

   if (mpi->flags & MP_IMGFLAG_DIRECT)
   {
      mp_msg(MSGT_VO, MSGL_ERR, "[pip] direct\n");
      return VO_TRUE;
   }

   if (0 != pip_format)
   {
      p96PIP_GetTags(My_Window, P96PIP_SourceBitMap, (uint32)&bm, TAG_DONE);
   }

   if (x > 0 || w < (int)image_width)
   {
      mp_msg(MSGT_VO, MSGL_ERR, "[pip] x=%d, w=%d\n", x, w);
      return VO_TRUE; //VO_ERROR;
   }

   if ((y + h) > (int)image_height)
   {
      int newh = image_height - y;
      mp_msg(MSGT_VO, MSGL_WARN, "[pip] y=%d h=%d -> h=%d\n", y, h, newh);
      h = newh;
   }

   if ((x + w) > (int)image_width)
   {
      int neww = image_width - x;
      mp_msg(MSGT_VO, MSGL_WARN, "[pip] x=%d w=%d -> w=%d\n", x, w, neww);
      w = neww;
   }

   if (x < 0 || y < 0 || (x + w) > (int)image_width || (y + h) > (int)image_height)
   {
      mp_msg(MSGT_VO, MSGL_ERR, "[pip] x=%d, y=%d %dx%d\n", x, y, w, h);
      return VO_TRUE; //VO_ERROR;
   }

   if (stride[0] < 0 || stride[1] < 0 || stride[2] < 0)
   {
      mp_msg(MSGT_VO, MSGL_ERR, "[pip] %d %d %d\n", stride[0], stride[1], stride[2]);
      return VO_TRUE; //VO_ERROR;
   }

   {
      int32 i, lock_h;

      if (! (lock_h = p96LockBitMap(bm, (uint8 *)&ri.RI, sizeof(struct YUVRenderInfo))))
      {
         // Unable to lock the BitMap -> do nothing
         mp_msg(MSGT_VO, MSGL_ERR, "[pip] locking failed\n");
         return VO_TRUE; //VO_ERROR;
      }

      srcY = image[0];
      srcU = image[1];
      srcV = image[2];
      if (RGBFB_YUV422CGX == pip_format)
      {
         dst = (uint8 *)ri.RI.Memory + y * ri.RI.BytesPerRow * 2;
      } else {
         dst = (uint8 *)ri.Planes[0] + y * ri.BytesPerRow[0];
      }
      // YUV420P and YUV410P only
      dstU = (uint8 *)ri.Planes[1] + y * ri.BytesPerRow[1] / 2;
      dstV = (uint8 *)ri.Planes[2] + y * ri.BytesPerRow[2] / 2;

      if ((RGBFB_YUV410P == pip_format || RGBFB_YUV420P == pip_format) && stride[0] == ri.BytesPerRow[0] && stride[0] == w)
      {
         if (RGBFB_YUV410P == pip_format)
         {
            int32 h2 = h / 4;
            int32 w2 = w / 4;

            gfxcopy(srcY, dst, w * h);

            for (i = 0 ; i < h2 ; i++)
            {
               gfxcopy_420P_410P(srcU, dstU, w2);
               gfxcopy_420P_410P(srcV, dstV, w2);

               srcU += stride[1] * 2;
               srcV += stride[2] * 2;
               dstU += ri.BytesPerRow[1];
               dstV += ri.BytesPerRow[2];
            }
         } else // if (RGBFB_YUV420P == pip_format)
         {
            int32 h2 = h / 2;
            int32 w2 = w / 2;

            gfxcopy(srcY, dst, w * h);

            for (i = 0 ; i < h2 ; i++)
            {
               gfxcopy(srcU, dstU, w2);
               gfxcopy(srcV, dstV, w2);

               srcU += stride[1];
               srcV += stride[2];
               dstU += ri.BytesPerRow[1];
               dstV += ri.BytesPerRow[2];
            }
         }
      } else {
         if (RGBFB_YUV422CGX == pip_format)
         {
            for (i = 0 ; i < h / 2 ; i++)
            {
               gfxcopy_420P_422CGX(srcY, srcY + stride[0], srcU, srcV, dst, dst + ri.RI.BytesPerRow, w / 8);

               srcY += stride[0] * 2;
               srcU += stride[1];
               srcV += stride[2];
               dst  += ri.RI.BytesPerRow * 2;
            }
         } else {
            for (i = 0 ; i < h ; i++)
            {
               gfxcopy(srcY, dst, w);
               srcY += stride[0];
               dst  += ri.BytesPerRow[0];
            }
         }

         if (RGBFB_YUV410P == pip_format)
         {
            int32 h2 = h / 4;
            int32 w2 = w / 4;

            for (i = 0 ; i < h2 ; i++)
            {
               gfxcopy_420P_410P(srcU, dstU, w2);
               gfxcopy_420P_410P(srcV, dstV, w2);

               srcU += stride[1] * 2;
               srcV += stride[2] * 2;
               dstU += ri.BytesPerRow[1];
               dstV += ri.BytesPerRow[2];
            }
         } else if (RGBFB_YUV420P == pip_format)
         {
            int32 h2 = h / 2;
            int32 w2 = w / 2;

            for (i = 0 ; i < h2 ; i++)
            {
               gfxcopy(srcU, dstU, w2);
               gfxcopy(srcV, dstV, w2);

               srcU += stride[1];
               srcV += stride[2];
               dstU += ri.BytesPerRow[1];
               dstV += ri.BytesPerRow[2];
            }
         }
      }

      p96UnlockBitMap(bm, lock_h);

      if (vo_osd_changed(0))
      {
         EraseRect(My_Window->RPort,
                              My_Window->BorderLeft, My_Window->BorderTop,
                              My_Window->Width - My_Window->BorderRight - 1, My_Window->Height - My_Window->BorderBottom - 1);
      }
   }

   return VO_TRUE;
}

/******************************** UNINIT    ******************************************/
static void uninit(void)
{
	unsigned long long int microsec;	

	if (benchmark_frame_cnt>0)
	{
		gettimeofday(&after,&dontcare);		// to get time after
		microsec = (after.tv_usec - before.tv_usec) +(1000000 * (after.tv_sec - before.tv_sec));
		printf("Internal COMP YUV FPS %0.00f\n",(float)  benchmark_frame_cnt / (float) (microsec / 1000 / 1000) );
	}

#ifdef USE_RGB
   if (R5G6B5_BitMap)
   {
      p96FreeBitMap(R5G6B5_BitMap);
      R5G6B5_BitMap = NULL;
   }

   if (Scaled_BitMap)
   {
      p96FreeBitMap(Scaled_BitMap);
      Scaled_BitMap = NULL;
   }
#endif

   Close_Window();
}

/******************************** CONTROL ******************************************/
static int control(uint32_t request, void *data)
{
   switch (request)
   {
      case VOCTRL_QUERY_FORMAT:
         return query_format(*(uint32 *)data);

      case VOCTRL_RESET:
         return VO_TRUE;

      case VOCTRL_GUISUPPORT:
         return VO_FALSE;

      case VOCTRL_FULLSCREEN:
         return VO_FALSE;

      case VOCTRL_PAUSE:
      case VOCTRL_RESUME:
         return VO_TRUE;

      case VOCTRL_DRAW_IMAGE:
         return draw_image(data);

      case VOCTRL_START_SLICE:
         return VO_NOTIMPL;

      case VOCTRL_ONTOP:
         {
            vo_ontop = !vo_ontop;

            Close_Window();
            ModeID = Open_Window(FALSE);
            if (INVALID_ID == ModeID)
            {
               return VO_FALSE;
            }

            return VO_TRUE;
         }

      case VOCTRL_UPDATE_SCREENINFO:
         if (!vo_screenwidth || !vo_screenheight)
         {
            vo_screenwidth  = 1024;
            vo_screenheight = 768;
         }
         aspect_save_screenres(vo_screenwidth, vo_screenheight);
         return VO_TRUE;

      case VOCTRL_GET_IMAGE:
      case VOCTRL_DRAW_EOSD:
      case VOCTRL_GET_EOSD_RES:
         return VO_NOTIMPL;
   }

   mp_msg(MSGT_VO, MSGL_ERR, "[pip] conttrol %d not supported\n", request);

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
         return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_OSD | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN | VOCAP_NOSLICES;

      default:
         mp_msg(MSGT_VO, MSGL_WARN, "[pip] format 0x%x '%c%c%c%c' not supported\n", format, format >> 24, (format >> 16) & 0xFF, (format >> 8) & 0xFF, format & 0xFF);
         return VO_FALSE;
   }
}
