/*
 *  vo_pip.c
 *  VO module for MPlayer AmigaOS
 *  Writen by Jörg strohmayer
 */

#ifdef __USE_INLINE__
#error AmigaOS 4.x code, fix your compiler options
#endif
#ifdef __ALTIVEC__
#warning No AltiVec code here :-(   Reimplement the gfxcopy*() functions
#endif

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

// OS specific
//#include "MPlayer-arexx.h"

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
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


static vo_info_t info =
{
    "AmigaOS video output",
    "pip",
    "Joerg Strohmayer",
    ""
};

LIBVO_EXTERN(pip)

/* ARexx
extern void put_command0(int);
extern struct ArexxHandle *rxHandler;
*/


static struct Window *My_Window;
#ifdef USE_RGB
static struct BitMap *R5G6B5_BitMap, *Scaled_BitMap;
#endif

enum
{
   NOBORDER,
   TINYBORDER,
   ALWAYSBORDER
};
static uint32 BorderMode = ALWAYSBORDER;

static uint32 image_width;
static uint32 image_height;
static uint32 image_format;
static uint32 out_format;
static uint32 window_width;
static uint32 window_height;
static char * window_title;
static uint32 pip_format;
#ifdef USE_RGB
static int16  yuv2rgb[5][255];
#endif
static uint32 ModeID;


/****************** MENU DEFINITION *****************/
static struct Menu *menu;
static struct NewMenu nm[] =
{
   /* Type,      Label,          CommKey, Flags, MutualExclude, UserData */

   { NM_TITLE,   "MPlayer",         NULL, 0,                  0, NULL },
      { NM_ITEM,    "Quit",         "Q",  0,                  0, NULL },
   { NM_TITLE,   "Play",            NULL, 0,                  0, NULL },
      { NM_ITEM,    "Play/Pause",   "P",  0,                  0, NULL },
      { NM_ITEM,    "Stop",         "S",  0,                  0, NULL },
   { NM_TITLE,   "Video Options",   NULL, 0,                  0, NULL },
      { NM_ITEM,    "Stay on Top",  NULL, MENUTOGGLE|CHECKIT, 0, NULL },
      { NM_ITEM,    "Cycle OSD",    "O",  0,                  0, NULL },
   { NM_TITLE,   "Audio Options",   NULL, 0,                  0, NULL },
      { NM_ITEM,    "Mute",         NULL, MENUTOGGLE|CHECKIT, 0, NULL },
   { NM_END,     NULL,              NULL, 0,                  0, NULL }
};
/****************** END MENU DEFINITION *****************/

static struct Gadget MyDragGadget =
{
   NULL,
   0,0,                                             // Pos
   -16,-16,                                         // Size
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

/***************************************************/
static APTR oldProcWindow;
static void procWindowInit(void)
{
   oldProcWindow = IDOS->SetProcWindow((APTR)-1);
}
__attribute__((section(".ctors"))) static void (*procWindowInitPtr)(void) USED = procWindowInit;
static void procWindowExit(void)
{
   IDOS->SetProcWindow(oldProcWindow);
}
__attribute__((section(".dtors"))) static void (*procWindowExitPtr)(void) USED = procWindowExit;
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
#ifdef USE_RGB
      if (0 == pip_format)
      {
         clut[0] = 0x00000000;
      } else
#endif
      {
         clut[0] = 0xFF110011;
      }
      for (i = 1 ; i < 256 ; i++)
      {
         clut[i] = (256-i)<<24 | (256-i) << 16 | (256-i) << 8 | (256-i);
      }
   }

#ifdef USE_RGB
   if (0 == pip_format)
   {
      IGraphics->BltBitMapTags(BLITA_Source, srca,
                               BLITA_SrcBytesPerRow, stride,
                               BLITA_SrcType, BLITT_CHUNKY,
                               BLITA_Dest, Scaled_BitMap,
                               BLITA_DestX, x,
                               BLITA_DestY, y,
                               BLITA_Width, w,
                               BLITA_Height, h,
                               BLITA_CLUT, clut,
                               BLITA_UseSrcAlpha, TRUE,
                               TAG_DONE);
   } else
#endif
   {
      IGraphics->BltBitMapTags(BLITA_Source, srca,
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
      IIntuition->ClearMenuStrip(My_Window);
#ifdef USE_RGB
      if (0 == pip_format)
      {
         IIntuition->CloseWindow(My_Window);
      } else
#endif
      {
         IP96->p96PIP_Close(My_Window);
      }
      My_Window = NULL;
   }
}

static uint32 Open_Window(uint32 hidden)
{
   struct Screen * My_Screen;

   ModeID = INVALID_ID;

   if ((My_Screen = IIntuition->LockPubScreen(NULL)))
   {
      APTR vi;
      ModeID = IGraphics->GetVPModeID(&My_Screen->ViewPort);

      vi = IGadTools->GetVisualInfoA(My_Screen, NULL);
      if (vi)
      {
         if (vo_ontop)
            nm[6].nm_Flags |= CHECKED;
         else
            nm[6].nm_Flags &= ~CHECKED;

         menu = IGadTools->CreateMenusA(nm,NULL);
         if (menu)
         {
            if (IGadTools->LayoutMenus(menu, vi, GTMN_NewLookMenus, TRUE, TAG_DONE))
            {
               struct DrawInfo *dri;

               if ((dri = IIntuition->GetScreenDrawInfo(My_Screen)))
               {
                  static struct IBox zoombox = {0, 0, 9999, 9999};
                  uint32 IDCMPFlags = IDCMP_MOUSEBUTTONS | IDCMP_MENUPICK | IDCMP_CLOSEWINDOW | IDCMP_RAWKEY | IDCMP_VANILLAKEY | IDCMP_EXTENDEDMOUSE | IDCMP_EXTENDEDKEYBOARD;
                  if (ALWAYSBORDER == BorderMode)
                  {
                     IDCMPFlags |= IDCMP_NEWSIZE;
                  }

                  zoombox.Width  = My_Screen->Width;
                  zoombox.Height = My_Screen->Height;

#ifdef USE_RGB
                  if (0 == pip_format)
                  {
                     My_Window = IIntuition->OpenWindowTags(NULL,
                        WA_CustomScreen,    (uint32) My_Screen,
                        ALWAYSBORDER == BorderMode ? WA_Title : WA_WindowName, GetWindowTitle(),
                        WA_ScreenTitle,     (uint32) "MPlayer",
                        WA_Left,            vo_dx + (NOBORDER == BorderMode ? 0 : My_Screen->WBorLeft),
                        WA_Top,             vo_dy + (ALWAYSBORDER == BorderMode ? My_Screen->WBorTop : 0),
                        WA_NewLookMenus,    TRUE,
                        WA_CloseGadget,     ALWAYSBORDER == BorderMode ? TRUE : FALSE,
                        WA_DepthGadget,     ALWAYSBORDER == BorderMode ? TRUE : FALSE,
                        WA_DragBar,         ALWAYSBORDER == BorderMode ? TRUE : FALSE,
                        WA_Borderless,      NOBORDER     == BorderMode ? TRUE : FALSE,
                        WA_SizeGadget,      ALWAYSBORDER == BorderMode ? TRUE : FALSE,
                        ALWAYSBORDER == BorderMode ? WA_SizeBBottom : TAG_IGNORE, TRUE,
                        WA_Activate,        TRUE,
                        WA_StayTop,         (vo_ontop==1) ? TRUE : FALSE,
                        WA_IDCMP,           IDCMPFlags,
                        ALWAYSBORDER == BorderMode ? TAG_IGNORE : WA_Gadgets, &MyDragGadget,
                        WA_InnerWidth,      window_width,
                        WA_InnerHeight,     window_height,
                        WA_MinWidth,        160,
                        WA_MinHeight,       100,
                        WA_MaxWidth,        My_Screen->Width,
                        WA_MaxHeight,       My_Screen->Height,
                        ALWAYSBORDER == BorderMode ? WA_Zoom : TAG_IGNORE, &zoombox,
//                        WA_DropShadows,     FALSE,
                        WA_Hidden,          hidden,
                     TAG_DONE);
                  } else
#endif
                  {
                     int32 sourcewidth = image_width;
                     int32 sourceheight = RGBFB_YUV410P == pip_format ? image_height & ~3 : RGBFB_YUV420P == pip_format ? image_height & ~1 : /*RGBFB_YUV422CGX*/ image_height;

                     My_Window = IP96->p96PIP_OpenTags(
                        P96PIP_SourceFormat, pip_format,
                        P96PIP_SourceWidth, sourcewidth,
                        P96PIP_SourceHeight, sourceheight,
                        P96PIP_NumberOfBuffers, 3,

                        WA_CustomScreen,    (uint32) My_Screen,
                        ALWAYSBORDER == BorderMode ? WA_Title : WA_WindowName, GetWindowTitle(),
                        WA_ScreenTitle,     (uint32) "MPlayer",
                        WA_Left,            vo_dx + (NOBORDER == BorderMode ? 0 : My_Screen->WBorLeft),
                        WA_Top,             vo_dy + (ALWAYSBORDER == BorderMode ? My_Screen->WBorTop : 0),
                        WA_NewLookMenus,    TRUE,
                        WA_CloseGadget,     ALWAYSBORDER == BorderMode ? TRUE : FALSE,
                        WA_DepthGadget,     ALWAYSBORDER == BorderMode ? TRUE : FALSE,
                        WA_DragBar,         ALWAYSBORDER == BorderMode ? TRUE : FALSE,
                        WA_Borderless,      NOBORDER     == BorderMode ? TRUE : FALSE,
                        WA_SizeGadget,      ALWAYSBORDER == BorderMode ? TRUE : FALSE,
                        ALWAYSBORDER == BorderMode ? WA_SizeBBottom : TAG_IGNORE, TRUE,
                        WA_Activate,        TRUE,
                        WA_StayTop,         (vo_ontop==1) ? TRUE : FALSE,
                        WA_IDCMP,           IDCMPFlags,
                        ALWAYSBORDER == BorderMode ? TAG_IGNORE : WA_Gadgets, &MyDragGadget,
                        WA_InnerWidth,      window_width,
                        WA_InnerHeight,     window_height,
                        WA_MinWidth,        160,
                        WA_MinHeight,       100,
                        WA_MaxWidth,        ~0,
                        WA_MaxHeight,       ~0,
                        ALWAYSBORDER == BorderMode ? WA_Zoom : TAG_IGNORE, &zoombox,
// enable for full screen                        WA_DropShadows,     FALSE,
                        WA_Hidden,          hidden,
                     TAG_DONE);

                     IIntuition->FreeScreenDrawInfo(My_Screen, dri);
                  }
               }

               if (My_Window)
               {
                  IIntuition->SetMenuStrip(My_Window, menu);
               }
            }
         }
         IGadTools->FreeVisualInfo(vi);
      }
      IIntuition->UnlockPubScreen(NULL, My_Screen);
   }

   if (!My_Window)
   {
      mp_msg(MSGT_VO, MSGL_FATAL, "[pip] Unable to open a window\n");
      return INVALID_ID;
   }

   if (ModeID != INVALID_ID)
   {
      IIntuition->ScreenToFront(My_Screen);
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

#ifdef USE_RGB
#define NUM_MODES 4
#else
#define NUM_MODES 3
#endif

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
#ifdef USE_RGB
             "    3: no overlay\n"
#endif
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
#ifdef USE_RGB
      case 3: pip_format = 0; break;
#endif
      default: mp_msg(MSGT_VO, MSGL_FATAL, "[pip] Unsupported 'mode=%d' option\n", opt_mode);
      return VO_ERROR;
   }

   if ((screen = IIntuition->LockPubScreen(NULL)))
   {
      vo_dwidth = vo_screenwidth  = screen->Width;
      vo_dheight = vo_screenheight = screen->Height;
      if (ALWAYSBORDER == BorderMode)
      {
         vo_dwidth  -= screen->WBorLeft + screen->WBorRight;
         vo_dheight -= screen->WBorTop + screen->Font->ta_YSize + 1 + screen->WBorBottom;
      }
      IIntuition->UnlockPubScreen(NULL, screen);
   }

   image_width = 640;
   image_height = 320;

   ModeID = Open_Window(TRUE);
   if (INVALID_ID == ModeID)
   {
      BOOL tested[NUM_MODES] = {FALSE};

      tested[opt_mode] = TRUE;

      for (i = 0 ; i < NUM_MODES ; i++)
      {
         if (tested[i]) continue;
         tested[i] = TRUE;

         switch (i)
         {
            case 0: pip_format = RGBFB_YUV410P;   break;
            case 1: pip_format = RGBFB_YUV420P;   break;
            case 2: pip_format = RGBFB_YUV422CGX; break;
#ifdef USE_RGB
            case 3: pip_format = 0; break;
#endif
         }

         ModeID = Open_Window(TRUE);
         if (INVALID_ID != ModeID)
         {
            break;
         }
      }
   }

   Close_Window();
   image_width = image_height = 0;

#ifdef USE_RGB
   for (i = 0 ; i < 256 ; i++)
   {
      yuv2rgb[0][i] = floor(round((i -  16) * 1.164));
      yuv2rgb[1][i] = floor(round((i - 128) * 1.596));
      yuv2rgb[2][i] = floor(round((i - 128) * 0.391));
      yuv2rgb[3][i] = floor(round((i - 128) * 0.821));
      yuv2rgb[4][i] = floor(round((i - 128) * 2.017));
   }
#endif

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
   
   vo_fs = VO_FALSE;

   window_width = d_width;
   window_height = d_height;

   ModeID = Open_Window(FALSE);
   if (INVALID_ID == ModeID)
   {
      return VO_ERROR;
   }

#ifdef USE_RGB
   if (0 == pip_format)
   {
      R5G6B5_BitMap = IP96->p96AllocBitMap(image_width, image_height, 16, BMF_DISPLAYABLE, NULL, RGBFB_R5G6B5);
      if (!R5G6B5_BitMap)
      {
         mp_msg(MSGT_VO, MSGL_FATAL, "[pip] Couldn't allocate bitmap\n");
         Close_Window();
         return VO_ERROR;
      }

      Scaled_BitMap = IP96->p96AllocBitMap(My_Window->WScreen->Width, My_Window->WScreen->Height, 32, BMF_DISPLAYABLE, My_Window->WScreen->RastPort.BitMap, RGBFB_A8R8G8B8);
      if (!Scaled_BitMap)
      {
         mp_msg(MSGT_VO, MSGL_FATAL, "[pip] Couldn't allocate bitmap\n");
         IP96->p96FreeBitMap(R5G6B5_BitMap);
         R5G6B5_BitMap = NULL;
         Close_Window();
         return VO_ERROR;
      }

   }
#endif

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


#ifdef USE_RGB
static void gfxcopy_YUV420P_RGB565(uint8 *sY0, uint8 *sY1, uint8 *sU, uint8 *sV, uint8 *d1, uint8 *d2, uint32 l)
{
   #define YUV2RGB2 R = (Y2+Cr)>>3;         G = (Y2-Cg1-Cg2)>>2;    B = (Y2+Cb)>>3; \
                    R = MAX(MIN(R, 31), 0); G = MAX(MIN(G, 63), 0); B = MAX(MIN(B, 31), 0)
   #define Y2(Y) Y2=yuv2rgb[0][Y]
   #define YUV2RGB(Y, U, V) Y2(Y); Cr=yuv2rgb[1][V]; Cg1=yuv2rgb[2][U]; Cg2=yuv2rgb[3][V]; Cb=yuv2rgb[4][U]; YUV2RGB2
   uint32 *d1L = (uint32 *)d1, *d2L = (uint32 *)d2, *Y0L = (uint32 *)sY0,
          *Y1L = (uint32 *)sY1, *UL = (uint32 *)sU, *VL = (uint32 *)sV, i, t;
   int32 Y2, Cr, Cg1, Cg2, Cb, R, G, B;
   uint16 *UW, *VW;
   for (i=l/4;i;i--)
   {
      uint32 Y0 = Y0L[0], Y1 = Y1L[0], U = UL[0], V = VL[0];
      YUV2RGB(Y0>>24    , U>>24     , V>>24     ); Y2(Y0>>16&0xFF); t  = (R<<11|G<<5|B)<<16;
      YUV2RGB2;                                                     t |=  R<<11|G<<5|B;
                                                   Y2(Y1>>24     ); d1L[0] = t;
      YUV2RGB2;                                    Y2(Y1>>16&0xFF); t  = (R<<11|G<<5|B)<<16;
      YUV2RGB2;                                                     t |=  R<<11|G<<5|B;
      YUV2RGB(Y0>>8&0xFF, U>>16&0xFF, V>>16&0xFF); Y2(Y0    &0xFF); d2L[0] = t;
                                                                    t  = (R<<11|G<<5|B)<<16;
      YUV2RGB2;                                                     t |=  R<<11|G<<5|B;
                                                   Y2(Y1>> 8&0xFF); d1L[1] = t;
      YUV2RGB2;                                    Y2(Y1    &0xFF); t  = (R<<11|G<<5|B)<<16;
      YUV2RGB2;                                                     t |=  R<<11|G<<5|B;
      Y0 = Y0L[1]; Y1 = Y1L[1];                                     d2L[1] = t;
      YUV2RGB(Y0>>24    , U>> 8&0xFF, V>> 8&0xFF); Y2(Y0>>16&0xFF); t  = (R<<11|G<<5|B)<<16;
      YUV2RGB2;                                                     t |=  R<<11|G<<5|B;
                                                   Y2(Y1>>24     ); d1L[2] = t;
      YUV2RGB2;                                    Y2(Y1>>16&0xFF); t  = (R<<11|G<<5|B)<<16;
      YUV2RGB2;                                                     t |=  R<<11|G<<5|B;
      YUV2RGB(Y0>>8&0xFF, U    &0xFF, V    &0xFF); Y2(Y0    &0xFF); d2L[2] = t;
                                                                    t  = (R<<11|G<<5|B)<<16;
      YUV2RGB2;                                                     t |=  R<<11|G<<5|B;
                                                   Y2(Y1>> 8&0xFF); d1L[3] = t;
      YUV2RGB2;                                    Y2(Y1    &0xFF); t  = (R<<11|G<<5|B)<<16;
      YUV2RGB2;                                                     t |=  R<<11|G<<5|B;
      Y0L += 2; Y1L += 2; UL++; VL++;                               d2L[3] = t;
      d1L += 4; d2L += 4;
   } 
   l &= 4-1; UW = (uint16 *)UL; VW = (uint16 *)VL;
   for (i=l/2;i;i--)
   {
      uint16 U = UW[0], V = VW[0]; uint32 Y0 = Y0L[0], Y1 = Y1L[0];
      YUV2RGB(Y0>>24    , U>>8  , V>>8  ); Y2(Y0>>16&0xFF); t  = (R<<11|G<<5|B)<<16;
      YUV2RGB2;                                             t |=  R<<11|G<<5|B;
                                           Y2(Y1>>24     ); d1L[0] = t;
      YUV2RGB2;                            Y2(Y1>>16&0xFF); t  = (R<<11|G<<5|B)<<16;
      YUV2RGB2;                                             t |=  R<<11|G<<5|B;
      YUV2RGB(Y0>>8&0xFF, U&0xFF, V&0xFF); Y2(Y0    &0xFF); d2L[0] = t;
                                                            t  = (R<<11|G<<5|B)<<16;
      YUV2RGB2;                                             t |=  R<<11|G<<5|B;
             ;                             Y2(Y1>> 8&0xFF); d1L[1] = t;
      YUV2RGB2;                            Y2(Y1    &0xFF); t  = (R<<11|G<<5|B)<<16;
      YUV2RGB2;                                             t |=  R<<11|G<<5|B;
      Y0L++; Y1L++; UW++; VW++;                             d2L[1] = t;
      d1L += 2; d2L += 2;
   }
   l &= 2-1;
   if (l)
   {
      uint8 U, V; uint16 Y0, Y1, *Y0W = (uint16 *)Y0L, *Y1W = (uint16 *)Y1L;
      sU = (uint8 *)UW; sV = (uint8 *)VW; Y0 = Y0W[0]; Y1 = Y1W[0]; U = sU[0]; V = sV[0];
      YUV2RGB(Y0>>8, U, V); Y2(Y0&0xFF); t  = (R<<11|G<<5|B)<<16;
      YUV2RGB2;                          t |=  R<<11|G<<5|B;
                            Y2(Y1>>8  ); d1L[0] = t;
      YUV2RGB2;             Y2(Y1&0xFF); t  = (R<<11|G<<5|B)<<16;
      YUV2RGB2;                          t |=  R<<11|G<<5|B;
                                         d2L[0] = t;
   }
}
#endif

/******************************** FLIP_PAGE ******************************************/
static void flip_page(void)
{
   if (!My_Window)
   {
      mp_msg(MSGT_VO, MSGL_ERR, "[pip] no window\n");
      return;
   }

#ifdef USE_RGB
   if (0 == pip_format)
   {
      uint32 left = My_Window->BorderLeft, top = My_Window->BorderTop;

      window_width  = My_Window->GZZWidth;
      window_height = My_Window->GZZHeight;

      IGraphics->BltBitMapRastPort(Scaled_BitMap, 0, 0, My_Window->RPort, left, top, window_width, window_height, 0xC0);
   } else
#endif
   {
      uint32 numbuffers = 0, WorkBuf/*, DispBuf*/;

      IP96->p96PIP_GetTags(My_Window, P96PIP_NumberOfBuffers, &numbuffers,  P96PIP_WorkBuffer, &WorkBuf, /*P96PIP_DisplayedBuffer, &DispBuf,*/ TAG_DONE);

      if (numbuffers > 1)
      {
         uint32 NextWorkBuf = (WorkBuf+1) % numbuffers;

         IP96->p96PIP_SetTags(My_Window, P96PIP_VisibleBuffer, WorkBuf, P96PIP_WorkBuffer, NextWorkBuf, TAG_DONE);
      }
   }
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
      IP96->p96PIP_GetTags(My_Window, P96PIP_SourceBitMap, (uint32)&bm, TAG_DONE);
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

#ifdef USE_RGB
   if (0 == pip_format)
   {
      static uint32 compFlags = COMPFLAG_SrcFilter | COMPFLAG_IgnoreDestAlpha | COMPFLAG_HardwareOnly;
      struct RenderInfo RGBri;
      uint32 left, top, scaleX, scaleY, error;
      int32 i, lock_h;
      if (!(lock_h = IP96->p96LockBitMap(R5G6B5_BitMap, (uint8 *)&RGBri, sizeof(RGBri))))
      {
         mp_msg(MSGT_VO, MSGL_ERR, "[pip] locking failed\n");
         return VO_TRUE; //VO_ERROR;
      }

      srcY = image[0];
      srcU = image[1];
      srcV = image[2];
      dst  = (uint8 *)RGBri.Memory + y * RGBri.BytesPerRow * 2;

      for (i = 0 ; i < h / 2 ; i++)
      {
         gfxcopy_YUV420P_RGB565(srcY, srcY + stride[0], srcU, srcV, dst, dst + RGBri.BytesPerRow, w / 2);

         srcY += stride[0] * 2;
         srcU += stride[1];
         srcV += stride[2];
         dst  += RGBri.BytesPerRow * 2;
      }

      IP96->p96UnlockBitMap(R5G6B5_BitMap, lock_h);

      left = My_Window->BorderLeft;
      top = My_Window->BorderTop;
      window_width  = My_Window->GZZWidth;
      window_height = My_Window->GZZHeight;

      scaleX = window_width  * COMP_FIX_ONE / image_width;
      scaleY = window_height * COMP_FIX_ONE / image_height;

      error = IGraphics->CompositeTags(COMPOSITE_Src, R5G6B5_BitMap, Scaled_BitMap,
                                       COMPTAG_DestX,      0,
                                       COMPTAG_DestY,      0,
                                       COMPTAG_OffsetX,    0,
                                       COMPTAG_OffsetY,    0,
                                       COMPTAG_DestWidth,  window_width,
                                       COMPTAG_DestHeight, window_height,
                                       COMPTAG_ScaleX,     scaleX,
                                       COMPTAG_ScaleY,     scaleY,
                                       COMPTAG_Flags,      compFlags,
                                       TAG_DONE);
      if (COMPERR_SoftwareFallback == error)
      {
         STRPTR boardName = "<NULL>";
         uint32 boardNumber = IP96->p96GetModeIDAttr(ModeID, P96IDA_BOARDNUMBER);

         compFlags = COMPFLAG_IgnoreDestAlpha;

         IP96->p96GetBoardDataTags(boardNumber, P96BD_BoardName, &boardName, TAG_DONE);
         mp_msg(MSGT_VO, MSGL_ERR, "[pip] '%s' doesn't support hardware compositing, using slow software fallback.\n", boardName);
      }
   } else
#endif
   {
      int32 i, lock_h;

      if (! (lock_h = IP96->p96LockBitMap(bm, (uint8 *)&ri.RI, sizeof(struct YUVRenderInfo))))
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

      IP96->p96UnlockBitMap(bm, lock_h);

      if (vo_osd_changed(0))
      {
         IGraphics->EraseRect(My_Window->RPort,
                              My_Window->BorderLeft, My_Window->BorderTop,
                              My_Window->Width - My_Window->BorderRight - 1, My_Window->Height - My_Window->BorderBottom - 1);
      }
   }

   return VO_TRUE;
}

/******************************** UNINIT    ******************************************/
static void uninit(void)
{
#ifdef USE_RGB
   if (R5G6B5_BitMap)
   {
      IP96->p96FreeBitMap(R5G6B5_BitMap);
      R5G6B5_BitMap = NULL;
   }

   if (Scaled_BitMap)
   {
      IP96->p96FreeBitMap(Scaled_BitMap);
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

            if (vo_ontop)
               nm[6].nm_Flags |= CHECKED;
            else
               nm[6].nm_Flags &= ~CHECKED;

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
   uint32 info_sig = 1L << (My_Window->UserPort)->mp_SigBit;

   if (IExec->SetSignal(0L, info_sig ) & info_sig)
   {
      struct IntuiMessage * IntuiMsg;
      uint32 msgClass;
      uint16 Code;

      while ((IntuiMsg = (struct IntuiMessage *)IExec->GetMsg(My_Window->UserPort)))
      {
         msgClass = IntuiMsg->Class;
         Code = IntuiMsg->Code;

         switch (msgClass)
         {
            case IDCMP_NEWSIZE:
#ifdef USE_RGB
               if (0 == pip_format)
               {
                  flip_page();
               }
#endif
               vo_dx = My_Window->LeftEdge - (NOBORDER == BorderMode ? 0 : My_Window->BorderLeft);
               vo_dy = My_Window->TopEdge - (ALWAYSBORDER == BorderMode ? My_Window->BorderTop : 0);
            break;

            case IDCMP_MOUSEBUTTONS:
            {
               static uint32 startSeconds, startMicros, Seconds, Micros;
               uint32 button_base = MOUSE_BTN0;
               if (Code & IECODE_UP_PREFIX)
               {
                  IIntuition->CurrentTime(&Seconds, &Micros);
                  if (IIntuition->DoubleClick(startSeconds, startMicros, Seconds, Micros))
                  {
                     button_base = MOUSE_BTN0_DBL; 
                  }
                  startSeconds = Seconds;
                  startMicros  = Micros;
               }
               switch (Code)
               {
                  case IECODE_LBUTTON   |IECODE_UP_PREFIX: mplayer_put_key(button_base + 0); break;
                  case IECODE_RBUTTON   |IECODE_UP_PREFIX: mplayer_put_key(button_base + 1); break;
                  case IECODE_MBUTTON   |IECODE_UP_PREFIX: mplayer_put_key(button_base + 2); break;
                  case IECODE_4TH_BUTTON|IECODE_UP_PREFIX: mplayer_put_key(button_base + 7); break;
                  case IECODE_5TH_BUTTON|IECODE_UP_PREFIX: mplayer_put_key(button_base + 8); break;
               }
            }
            break;

            case IDCMP_MENUPICK:
               while (Code != MENUNULL)
               {
                  struct MenuItem  *item = IIntuition->ItemAddress(menu, Code);

                  switch (MENUNUM(Code))
                  {
                     case 0:
                        switch (ITEMNUM(Code))
                        {
                           case 0:  /* Quit */
                              mplayer_put_key(KEY_ESC);
                           break;
                        }
                     break;

                     case 1:
                        switch (ITEMNUM(Code))
                        {
                           case 0:  /* pause/play */
                              mplayer_put_key('p');
                           break;

                           case 1:  /* Quit */
                              mp_input_parse_and_queue_cmds("stop");
                           break;
                        }
                     break;

                     case 2:
                        switch (ITEMNUM(Code))
                        {
                           case 0:
                              mp_input_parse_and_queue_cmds("ontop");
                           break;

                           case 1:
                              mplayer_put_key('o');
                           break;
                        }
                     break;

                     case 4:
                        mp_input_parse_and_queue_cmds("mute");
                     break;
                  }

                  /* Handle multiple selection */
                  Code = item->NextSelect;
               }
            break;

            case IDCMP_CLOSEWINDOW:
                mplayer_put_key(KEY_ESC);
            break;

            case IDCMP_RAWKEY:
               switch (Code)
               {
                  case RAWKEY_BACKSPACE: mplayer_put_key(KEY_BACKSPACE); break;
                  case RAWKEY_TAB:       mplayer_put_key(KEY_TAB);       break;
                  case RAWKEY_RETURN:    mplayer_put_key(KEY_ENTER);     break;
                  case RAWKEY_ESC:       mplayer_put_key(KEY_ESC);       break;
                  case RAWKEY_DEL:       mplayer_put_key(KEY_DELETE);    break;
                  case RAWKEY_INSERT:    mplayer_put_key(KEY_INSERT);    break;
                  case RAWKEY_HOME:      mplayer_put_key(KEY_HOME);      break;
                  case RAWKEY_END:       mplayer_put_key(KEY_END);       break;
                  case RAWKEY_PAGEUP:    mplayer_put_key(KEY_PAGE_UP);   break;
                  case RAWKEY_PAGEDOWN:  mplayer_put_key(KEY_PAGE_DOWN); break;
                  case RAWKEY_CRSRRIGHT: mplayer_put_key(KEY_RIGHT);     break;
                  case RAWKEY_CRSRLEFT:  mplayer_put_key(KEY_LEFT);      break;
                  case RAWKEY_CRSRDOWN:  mplayer_put_key(KEY_DOWN);      break;
                  case RAWKEY_CRSRUP:    mplayer_put_key(KEY_UP);        break;
                  case 0x0F:             mplayer_put_key(KEY_KP0);       break;
                  case 0x1D:             mplayer_put_key(KEY_KP1);       break;
                  case 0x1E:             mplayer_put_key(KEY_KP2);       break;
                  case 0x1F:             mplayer_put_key(KEY_KP3);       break;
                  case 0x2D:             mplayer_put_key(KEY_KP4);       break;
                  case 0x2E:             mplayer_put_key(KEY_KP5);       break;
                  case 0x2F:             mplayer_put_key(KEY_KP6);       break;
                  case 0x3D:             mplayer_put_key(KEY_KP7);       break;
                  case 0x3E:             mplayer_put_key(KEY_KP8);       break;
                  case 0x3F:             mplayer_put_key(KEY_KP9);       break;
                  case 0x3C:             mplayer_put_key(KEY_KPDEC);     break;
                  case RAWKEY_ENTER:     mplayer_put_key(KEY_KPENTER);   break;
                  case RAWKEY_LCTRL:     mplayer_put_key(KEY_CTRL);      break;
                  case RAWKEY_LSHIFT:    mplayer_put_key(KEY_CTRL + 1);  break;
                  case RAWKEY_RSHIFT:    mplayer_put_key(KEY_CTRL + 2);  break;
                  case RAWKEY_CAPSLOCK:  mplayer_put_key(KEY_CTRL + 3);  break;
                  case RAWKEY_LALT:      mplayer_put_key(KEY_CTRL + 4);  break;
                  case RAWKEY_RALT:      mplayer_put_key(KEY_CTRL + 5);  break;
                  case RAWKEY_LCOMMAND:  mplayer_put_key(KEY_CTRL + 6);  break;
                  case RAWKEY_RCOMMAND:  mplayer_put_key(KEY_CTRL + 7);  break;
                  case RAWKEY_F1:        mplayer_put_key(KEY_F + 1);     break;
                  case RAWKEY_F2:        mplayer_put_key(KEY_F + 2);     break;
                  case RAWKEY_F3:        mplayer_put_key(KEY_F + 3);     break;
                  case RAWKEY_F4:        mplayer_put_key(KEY_F + 4);     break;
                  case RAWKEY_F5:        mplayer_put_key(KEY_F + 5);     break;
                  case RAWKEY_F6:        mplayer_put_key(KEY_F + 6);     break;
                  case RAWKEY_F7:        mplayer_put_key(KEY_F + 7);     break;
                  case RAWKEY_F8:        mplayer_put_key(KEY_F + 8);     break;
                  case RAWKEY_F9:        mplayer_put_key(KEY_F + 9);     break;
                  case RAWKEY_F10:       mplayer_put_key(KEY_F + 10);    break;
                  case RAWKEY_F11:       mplayer_put_key(KEY_F + 11);    break;
                  case RAWKEY_F12:       mplayer_put_key(KEY_F + 12);    break;
                  case RAWKEY_MENU:      mplayer_put_key(KEY_MENU);      break;
                  case RAWKEY_MEDIA_PLAY_PAUSE: mplayer_put_key(KEY_PLAYPAUSE); break;
                  case RAWKEY_MEDIA_STOP:       mplayer_put_key(KEY_STOP);      break;
                  case RAWKEY_MEDIA_NEXT_TRACK: mplayer_put_key(KEY_NEXT);      break;
                  case RAWKEY_MEDIA_PREV_TRACK: mplayer_put_key(KEY_PREV);      break;
               }
            break;

            case IDCMP_VANILLAKEY:
               switch ( Code )
               {
                  case  '\e':
                     mplayer_put_key(KEY_ESC);
                  break;

                  default:
                     if (0 != Code) mplayer_put_key(Code);
                  break;
               }
            break;

            case IDCMP_EXTENDEDMOUSE:
               if (IMSGCODE_INTUIWHEELDATA == Code)
               {
                  struct IntuiWheelData *wd = IntuiMsg->IAddress;
                  if      (wd->WheelY < 0) mplayer_put_key(MOUSE_BTN3);
                  else if (wd->WheelY > 0) mplayer_put_key(MOUSE_BTN4);
                  else if (wd->WheelX < 0) mplayer_put_key(MOUSE_BTN5);
                  else if (wd->WheelX > 0) mplayer_put_key(MOUSE_BTN6);
               }
            break;

            case IDCMP_EXTENDEDKEYBOARD:
               if (IMSGCODE_INTUIRAWKEYDATA == Code)
               {
                  struct IntuiRawKeyData *rkd = IntuiMsg->IAddress;
                  if (IESUBCLASS_HID_CONSUMER_DOWN_RAWKEY == rkd->Class)
                  {
                     switch (rkd->Code)
                     {
                        case 0x66: mplayer_put_key(KEY_POWER);       break;
                        case 0x7F: mplayer_put_key(KEY_MUTE);        break;
                        case 0x80: mplayer_put_key(KEY_VOLUME_UP);   break;
                        case 0x81: mplayer_put_key(KEY_VOLUME_DOWN); break;
                     }
                  }
               }
            break;
         }

         IExec->ReplyMsg(&IntuiMsg->ExecMessage);
      }
   }

#if 0
/* ARexx */
   if(IExec->SetSignal(0L,rxHandler->sigmask) & rxHandler->sigmask)
   {
      IIntuition->IDoMethod(rxHandler->rxObject,AM_HANDLEEVENT);
   }
/* ARexx */
#endif
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
