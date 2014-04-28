/*
 *  gfx_common.c
 *  common GFX function for MPlayer MorphOS
 *  Written by DET Nicolas
 *  Maintained and updated by Fabien Coeurjoly
*/


#define SYSTEM_PRIVATE

#include <exec/types.h>

#include <proto/exec.h>
#include <proto/timer.h>
#include <proto/dos.h>

#ifndef __amigaos4__
#include <proto/alib.h>
#endif

#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/cybergraphics.h>
#include <proto/layers.h>
#include <proto/wb.h>
#include <proto/keymap.h>

#include <cybergraphx/cybergraphics.h>
#include <graphics/gfxbase.h>

#ifndef __amigaos4__
#include <intuition/extensions.h>
#endif

#include <libraries/gadtools.h>

#if defined(__morphos__) || defined(__aros__)
#include <devices/rawkeycodes.h>
#endif

#ifdef __amigaos4__
#include <libraries/keymap.h>
#include <proto/Picasso96API.h>
#endif

#ifndef   DOS_RDARGS_H
#include <dos/rdargs.h>
#endif


#ifndef   INTUITION_CLASSES_H
#include <intuition/classes.h>
#endif

#include <devices/inputevent.h>
#include <utility/hooks.h>
#include <workbench/workbench.h>
#include <workbench/startup.h>

#include <osdep/keycodes.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __morphos__
#include "morphos_stuff.h"
#endif

#include "mp_msg.h"
#include "video_out.h"
#include "mp_fifo.h"
#include "config.h"
#include "input/input.h"
#include "input/mouse.h"

#ifdef CONFIG_GUI
#include "gui/interface.h"
#include "gui/morphos/gui.h"
#endif

extern APTR window_mx;

// Set here, and use in the vo_gfx_**
char * gfx_monitor = NULL;
ULONG WantedModeID = 0;

// Blank pointer
UWORD *EmptyPointer = NULL;

#include <mplayer-arexx.h>

/* ARexx */
extern struct ArexxHandle *rxHandler; 
/* ARexx */


extern uint32_t is_fullscreen;
char PubScreenName[128] = "";

#define NOBORDER			0
#define TINYBORDER			1
#define DISAPBORDER			2
#define ALWAYSBORDER		3
ULONG gfx_BorderMode = ALWAYSBORDER;

// Timer for the pointer...
ULONG p_mics1=0, p_mics2=0, p_secs1=0, p_secs2=0;
BOOL mouse_hidden=FALSE;

static BOOL mouseonborder;

void gfx_ShowMouse(struct Screen * screen, struct Window * window, ULONG enable);

#ifdef __amigaos4__

#define RAWKEY_ESCAPE       0x45
#define RAWKEY_CONTROL     0x63
#define RAWKEY_DELETE     0x46
#define RAWKEY_KP_ENTER     0x43
#define RAWKEY_KP_7      0x3D
#define RAWKEY_KP_8      0x3E
#define RAWKEY_KP_9      0x3F
#define RAWKEY_KP_4      0x2D
#define RAWKEY_KP_5      0x2E
#define RAWKEY_KP_6      0x2F
#define RAWKEY_KP_1      0x1D
#define RAWKEY_KP_2      0x1E
#define RAWKEY_KP_3      0x1F
#define RAWKEY_KP_0      0x0F
#define RAWKEY_LAMIGA     0x66
#define RAWKEY_RAMIGA     0x67

#endif

/* appwindow stuff */
struct MsgPort *awport;
struct AppWindow *appwin;
ULONG  appwinsig, appid = 1, appuserdata = 0;

/* Drag/Size gadgets definitions */
static struct Gadget MyDragGadget =
{
	NULL,
	0,0,								// Pos
	0,0,								// Size
	0,									// Flags
	0,									// Activation
	GTYP_WDRAGGING,						// Type
	NULL,								// GadgetRender
	NULL,								// SelectRender
	NULL,								// GadgetText
	0L,									// Obsolete
	NULL,								// SpecialInfo
	0,									// Gadget ID
	NULL								// UserData
};

static struct Gadget MySizeGadget =
{
	NULL,
	0,0,								// Pos
	0,0,								// Size
	0,									// Flags
	0,									// Activation
	GTYP_SIZING ,		 				// Type
	NULL,								// GadgetRender
	NULL,								// SelectRender
	NULL,								// GadgetText
	0L,									// Obsolete
	NULL,								// SpecialInfo
	0,									// Gadget ID
	NULL								// UserData
};

/****************************/
static const char *Messages[]=
{ 
	"Le dormeur doit se réveiller.\n",
	"Béni soit le faiseur et son nom.\n",
	"Uzul parle moi de ton monde natal.\n",
};

void gfx_Message(void)
{
#ifndef __amigaos4__
	struct timeval tv;
	GetSysTime(&tv);
	if ( ( tv.tv_secs % 60) < 5)  Printf("Message: %s\n", Messages[ tv.tv_micro % 3 ] );
#endif

#ifdef __amigaos4__
	struct TimeVal tv;
	GetSysTime(&tv);
	if ( ( tv.Seconds % 60) < 5)  Printf("Message: %s\n", Messages[ tv.Microseconds % 3 ] );
#endif

}

/***************************/
BOOL gfx_GiveArg(const char *arg)
{
	STRPTR PtrArg = (STRPTR) arg;

	// Default settings
	gfx_monitor 		= NULL;
	gfx_BorderMode 	= ALWAYSBORDER;

	if ( arg && strlen(arg) )
	{
		// Some parsing :-(
		STRPTR PtrSeparator = index(PtrArg, ':' );

		if (!PtrSeparator)
		{
			// Ok no ":", so we have only 1 arg and it's the monitor
			gfx_monitor = AllocVec( strlen(arg)+1 , MEMF_ANY);
			if (!gfx_monitor) return FALSE;
			strcpy( gfx_monitor, PtrArg );
		}
		else
		{
			STRPTR ptr;

			// Ok we have several args
			ULONG FirstArgSize = PtrSeparator - PtrArg + 1;

			if (FirstArgSize > 1)
			{
				gfx_monitor = AllocVec( FirstArgSize , MEMF_ANY);
				if (!gfx_monitor) return FALSE;
				memcpy( gfx_monitor, PtrArg, FirstArgSize); // PtrArg - PtrSeparator -> Size of the ard
				gfx_monitor[FirstArgSize -1] = '\0';
			}

			// Second arg is the Bordermode
			PtrArg = PtrSeparator + 1;
			if (!*PtrArg) goto end;

			if (!(memcmp(PtrArg, "NOBORDER", strlen("NOBORDER") ) ) ) gfx_BorderMode = NOBORDER;
			else if (!(memcmp(PtrArg, "TINYBORDER", strlen("TINYBORDER") ) ) ) gfx_BorderMode = TINYBORDER;
			else if (!(memcmp(PtrArg, "ALWAYSBORDER", strlen("ALWAYSBORDER") ) ) ) gfx_BorderMode = ALWAYSBORDER;
			else if (!(memcmp(PtrArg, "DISAPBORDER", strlen("DISAPBORDER") ) ) ) gfx_BorderMode = DISAPBORDER;

#ifdef CONFIG_GUI
			if(use_gui && mygui->embedded) /* Check modes supported by GUI */
			{
				if(gfx_BorderMode == NOBORDER || gfx_BorderMode == TINYBORDER || gfx_BorderMode == DISAPBORDER)
				{
					gfx_BorderMode = ALWAYSBORDER;
				}
			}
#endif

			/* mode id ? */
			if((ptr = strstr(PtrArg, "MODEID=")))
			{
				TEXT mode[64];
				STRPTR PtrSeparator = index(ptr, ':' );

				if (!PtrSeparator)
				{
					strcpy( mode, ptr + strlen("MODEID="));
					WantedModeID = strtol(mode, NULL, 16);
				}
				else
				{
					// Ok we have several args
					ULONG ArgSize = PtrSeparator - ptr - strlen("MODEID=") + 1;

					if (ArgSize > 1)
					{
						memcpy( mode, ptr + strlen("MODEID="), ArgSize);
						mode[ArgSize -1] = '\0';
						WantedModeID = strtol(mode, NULL, 16);
					}
				}
			}
			else if((ptr = strstr(PtrArg, "PUBSCREEN=")))
			{
				TEXT mode[64];
				STRPTR PtrSeparator = index(ptr, ':' );

				if (!PtrSeparator)
				{
					strcpy( mode, ptr + strlen("PUBSCREEN="));
				}
				else
				{
					// Ok we have several args
					ULONG ArgSize = PtrSeparator - ptr - strlen("PUBSCREEN=") + 1;

					if (ArgSize > 1)
					{
						memcpy( mode, ptr + strlen("PUBSCREEN="), ArgSize);
						mode[ArgSize -1] = '\0';
					}
				}

				strcpy(PubScreenName, mode);
			}
	
			// no more arg 
		}
	}
end:
	return TRUE;
}

/***************************/
VOID gfx_ReleaseArg(VOID)
{
	if (gfx_monitor)
	{
		FreeVec(gfx_monitor);
		gfx_monitor = NULL;
	}
}

/***************************/
// Our trasparency hook
static BOOL ismouseon(struct Window *window);

#ifndef __amigaos4__
static void MyTranspHook(struct Hook *hook,struct Window *window,struct TransparencyMessage *msg);

struct Hook transphook = 
{
   {NULL, NULL},
   (ULONG (*) (VOID) ) HookEntry,
   (APTR) MyTranspHook,
   NULL
};

#endif

/***************************/

void UpdateGadgets(struct Window * My_Window, int WindowWidth, int WindowHeight)
{
	MyDragGadget.LeftEdge = (WORD) 0;
	MyDragGadget.TopEdge  = (WORD) 0;
	MyDragGadget.Width 	  = (WORD) WindowWidth-10;
	MyDragGadget.Height   = (WORD) 30;

	// But do not say if this func can fail (or give the error return code
	// then I assume it always success
	AddGadget(My_Window, &MyDragGadget, 0);

	MySizeGadget.LeftEdge = (WORD) WindowWidth-10;
	MySizeGadget.TopEdge  = (WORD) 0;
	MySizeGadget.Width 	  = (WORD) 10;
	MySizeGadget.Height   = (WORD) WindowHeight;

	// But do not say if this func can fail (or give the error return code
	// then I assume it always success
	AddGadget(My_Window, &MySizeGadget, 0);

	RefreshGadgets(&MyDragGadget, My_Window, 0);
	RefreshGadgets(&MySizeGadget, My_Window, 0);
}

void RemoveGadgets(struct Window * My_Window)
{
	RemoveGadget(My_Window, &MyDragGadget);
	RemoveGadget(My_Window, &MySizeGadget);
}

void gfx_StartWindow(struct Window *My_Window)
{
	switch (gfx_BorderMode)
	{
		case NOBORDER:
		case TINYBORDER:
		{
			LONG WindowWidth;
			LONG WindowHeight;

			WindowHeight=My_Window->Height - (My_Window->BorderBottom + My_Window->BorderTop);
			WindowWidth=My_Window->Width - (My_Window->BorderLeft + My_Window->BorderRight);

			UpdateGadgets(My_Window, WindowWidth, WindowHeight);
		}
		break;
		case DISAPBORDER:
		{

#ifndef __amigaos4__
			struct TagItem tags[] =
			{
				{TRANSPCONTROL_REGIONHOOK, (ULONG) &transphook},
				{TAG_DONE}
			};

			TransparencyControl(My_Window, TRANSPCONTROLMETHOD_INSTALLREGIONHOOK, tags);
#endif
		}
	}
}

/***************************/
void gfx_StopWindow(struct Window *My_Window)
{
	if(My_Window)
	{
		switch (gfx_BorderMode)
		{
			case TINYBORDER:
			case NOBORDER:
				RemoveGadgets(My_Window);
				break;
		}
	}
}

void gfx_HandleBorder(struct Window *My_Window, ULONG handle_mouse)
{
	if (gfx_BorderMode == DISAPBORDER && !is_fullscreen)
	{
		ULONG toggleborder = FALSE;

		if (handle_mouse)
		{
			BOOL mouse = ismouseon(My_Window);
			if (mouse != mouseonborder)
			{
				toggleborder = TRUE;
			}
			mouseonborder = mouse;
		}
		else
		{
			toggleborder = TRUE;
		}

		if(toggleborder)
		{
#ifndef __amigaos4__
			TransparencyControl(My_Window,TRANSPCONTROLMETHOD_UPDATETRANSPARENCY,NULL);
#endif
		}
	}
}

/***************************/
void gfx_Start(struct Window *My_Window, BOOL FullScreen)
{
	if((awport = CreateMsgPort()))
	{
		appwin = AddAppWindow(appid, appuserdata, My_Window, awport, NULL);

		if(appwin)
		{
			appwinsig = 1L << awport->mp_SigBit;
		}
	}
}

/***************************/
void gfx_Stop(struct Window *My_Window)
{
	struct AppMessage *amsg;

	if(appwin)
	{
		RemoveAppWindow(appwin);
		appwin = NULL;
	}

	if(awport)
	{
		while((amsg = (struct AppMessage *)GetMsg(awport)))
			ReplyMsg((struct Message *)amsg);
		DeleteMsgPort(awport);
		awport = NULL;
	}
}

/***************************************************/

int secs,secs2,mics,mics2;

BOOL gfx_CheckEvents(struct Screen *My_Screen, struct Window *My_Window, uint32_t *window_height, uint32_t *window_width,
	uint32_t *window_left, uint32_t *window_top )
{
	ULONG retval = FALSE;
	ULONG info_sig=1L;

	if ( ! MutexAttempt( window_mx ) ) return retval;

	if (My_Window == NULL)
	{
		MutexRelease(window_mx);
		return retval;
	}

	info_sig=1L<<(My_Window->UserPort)->mp_SigBit;

#ifdef CONFIG_GUI
if(!use_gui)
{
#endif

/* ARexx */
    if(SetSignal(0L,rxHandler->sigmask) & rxHandler->sigmask)
    {
        IDoMethod(rxHandler->rxObject,AM_HANDLEEVENT);
    }
/* ARexx */

#ifdef CONFIG_GUI
}
#endif

	if (is_fullscreen && !mouse_hidden)
	{
		if (!p_secs1 && !p_mics1)
		{
			CurrentTime(&p_secs1, &p_mics1);
		}
		else
		{
			CurrentTime(&p_secs2, &p_mics2);
			if (p_secs2-p_secs1>=2)
			{
				// Ok, let's hide ;)
				gfx_ShowMouse(My_Screen, My_Window, FALSE);
				p_secs1=p_secs2=p_mics1=p_mics2=0;
			}
		}
	}
	
	if(SetSignal(0L,info_sig ) & info_sig) 
	{ // If an event -> Go Go GO !

		struct IntuiMessage * IntuiMsg;
		ULONG Class;
		UWORD Code;
		UWORD Qualifier;
		int MouseX, MouseY;
		struct IntuiWheelData *mouse_wheel;

		while ( ( IntuiMsg = (struct IntuiMessage *) GetMsg( My_Window->UserPort ) ) )
		{
			Class     = IntuiMsg->Class;
			Code      = IntuiMsg->Code;
			Qualifier = IntuiMsg->Qualifier;
			MouseX    = IntuiMsg->MouseX;
			MouseY    = IntuiMsg->MouseY;
			mouse_wheel = (struct IntuiWheelData *) IntuiMsg -> IAddress;

			ReplyMsg( (struct Message *) IntuiMsg);

			switch( Class )
			{
				case IDCMP_CLOSEWINDOW: 
					mplayer_put_key(KEY_ESC);   // that for whole exit when video-window close

					//mp_input_queue_cmd(mp_input_parse_cmd("stop"));  // for close 

					break;

				case IDCMP_ACTIVEWINDOW:
#ifndef __amigaos4__
					if (gfx_BorderMode == DISAPBORDER) TransparencyControl(My_Window,TRANSPCONTROLMETHOD_UPDATETRANSPARENCY,NULL);
#endif
					break;

				case IDCMP_INACTIVEWINDOW:
#ifndef __amigaos4__
					if (gfx_BorderMode == DISAPBORDER) TransparencyControl(My_Window,TRANSPCONTROLMETHOD_UPDATETRANSPARENCY,NULL);
#endif
					break;

				case IDCMP_MOUSEBUTTONS:
					printf("%s:%ld\n",__FUNCTION__,__LINE__);
					// Blanks pointer stuff
					if (is_fullscreen && mouse_hidden)
					{
						gfx_ShowMouse(My_Screen, My_Window, TRUE);
					}

					switch(Code)
					{
						case SELECTDOWN:

//							mplayer_put_key(MOUSE_BTN0 | MP_KEY_DOWN);

							if (!secs&&!mics)
							{
								secs=IntuiMsg->Seconds;
								mics=IntuiMsg->Micros;
								break; // no need to test double-click
							}
							else
							{
								secs2=IntuiMsg->Seconds;
								mics2=IntuiMsg->Micros;
							}

							if ( DoubleClick(secs, mics, secs2, mics2) )
							{
                                				put_command0(MP_CMD_VO_FULLSCREEN);
								secs = mics = secs2 = mics2 = 0;
								p_secs1 = p_secs2 = p_mics2 = p_mics1 = 0;
								mouse_hidden=FALSE;
							}
							else
							{
								secs = secs2;
								mics = mics2;
							}
							break;
/*
						case SELECTUP:
							mplayer_put_key(MOUSE_BTN0 );
							break;
*/
						case MENUDOWN:
							mplayer_put_key(MOUSE_BTN1 | MP_KEY_DOWN);
							break;

						case MENUUP:
							mplayer_put_key(MOUSE_BTN1);
							break;

						case MIDDLEDOWN:
							mplayer_put_key(MOUSE_BTN2 | MP_KEY_DOWN);
							#ifdef CONFIG_GUI
							if (use_gui)
							{
								gui_show_gui ^= TRUE;
								guiGetEvent(guiShowPanel, gui_show_gui);
							}
							#endif
							break;

						case MIDDLEUP:
							mplayer_put_key(MOUSE_BTN2);
							break;

						default:
							;
					}

					break;
    
				case IDCMP_MOUSEMOVE:
					{
						char cmd_str[40];
						
						// Blanks pointer stuff
						if (is_fullscreen)
						{
							if (mouse_hidden)
							{
								gfx_ShowMouse(My_Screen, My_Window, TRUE);
								/*
								#ifdef CONFIG_GUI
								if (use_gui)
									guiGetEvent(guiShowPanel, TRUE);
								#endif
								*/
							}
						}
#ifndef __amigaos4__						
						if (gfx_BorderMode == DISAPBORDER)
						{
							BOOL mouse = ismouseon(My_Window);

							if (mouse != mouseonborder) TransparencyControl(My_Window, TRANSPCONTROLMETHOD_UPDATETRANSPARENCY,NULL);
								mouseonborder = mouse;
						}
#endif
						sprintf(cmd_str,"set_mouse_pos %i %i", MouseX-My_Window->BorderLeft, MouseY-My_Window->BorderTop);
						mp_input_queue_cmd(mp_input_parse_cmd(cmd_str));
					}

					break;

				case IDCMP_REFRESHWINDOW:
					BeginRefresh(My_Window);
					EndRefresh(My_Window,TRUE);
					break;

				case IDCMP_CHANGEWINDOW:
					*window_left = My_Window->LeftEdge;
					*window_top  = My_Window->TopEdge;
					*window_height  = My_Window->Height - (My_Window->BorderBottom + My_Window->BorderTop);
					*window_width  = My_Window->Width - (My_Window->BorderLeft + My_Window->BorderRight);

					vo_dwidth = *window_width;
					vo_dheight = *window_height;

					if(gfx_BorderMode == NOBORDER ||gfx_BorderMode == TINYBORDER)
					{
						RemoveGadgets(My_Window);
						UpdateGadgets(My_Window, *window_width, *window_height);
					}

					retval = TRUE;
					break;
#ifdef __amigaos4__
				case IDCMP_EXTENDEDMOUSE:
					if (mouse_wheel -> WheelY < 0)
					{
						mplayer_put_key(KEY_RIGHT); break;
					}
					else	if (mouse_wheel->WheelY>0)
					{
						mplayer_put_key(KEY_LEFT); break;
					}
					break;
#endif

				case IDCMP_RAWKEY:
					 switch ( Code )
					 {
						 case  RAWKEY_ESCAPE:    mplayer_put_key(KEY_ESC); break;
						 case  RAWKEY_PAGEDOWN:  mplayer_put_key(KEY_PGDWN); break;
						 case  RAWKEY_PAGEUP:    mplayer_put_key(KEY_PGUP); break;

#ifndef __amigaos4__
						 case  NM_WHEEL_UP:      // MouseWheel rulez !
						 case  NM_WHEEL_DOWN:

						 case  RAWKEY_RIGHT:     mplayer_put_key(KEY_RIGHT); break;
						 case  RAWKEY_LEFT:      mplayer_put_key(KEY_LEFT); break;
						 case  RAWKEY_UP:        mplayer_put_key(KEY_UP); break;
						 case  RAWKEY_DOWN:      mplayer_put_key(KEY_DOWN); break;
#endif

#ifdef __amigaos4__
						 case  RAWKEY_CRSRRIGHT:     mplayer_put_key(KEY_RIGHT); break;
						 case  RAWKEY_CRSRLEFT:      mplayer_put_key(KEY_LEFT); break;
						 case  RAWKEY_CRSRUP:        mplayer_put_key(KEY_UP); break;
						 case  RAWKEY_CRSRDOWN:      mplayer_put_key(KEY_DOWN); break;
#endif

						 case  RAWKEY_F1:        mplayer_put_key(KEY_F+1); break;
						 case  RAWKEY_F2:        mplayer_put_key(KEY_F+2); break;
						 case  RAWKEY_F3:        mplayer_put_key(KEY_F+3); break;
						 case  RAWKEY_F4:        mplayer_put_key(KEY_F+4); break;
						 case  RAWKEY_F5:        mplayer_put_key(KEY_F+5); break;
						 case  RAWKEY_F6:        mplayer_put_key(KEY_F+6); break;
						 case  RAWKEY_F7:        mplayer_put_key(KEY_F+7); break;
						 case  RAWKEY_F8:        mplayer_put_key(KEY_F+8); break;
						 case  RAWKEY_F9:        mplayer_put_key(KEY_F+9); break;
						 case  RAWKEY_F10:       mplayer_put_key(KEY_F+10); break;
						 case  RAWKEY_F11:       mplayer_put_key(KEY_F+11); break;
						 case  RAWKEY_F12:       mplayer_put_key(KEY_F+12); break;
						 case  RAWKEY_RETURN:    mplayer_put_key(KEY_ENTER); break;
						 case  RAWKEY_TAB:       mplayer_put_key(KEY_TAB); break;
						 case  RAWKEY_CONTROL:   mplayer_put_key(KEY_CTRL); break;
						 case  RAWKEY_BACKSPACE: mplayer_put_key(KEY_BACKSPACE); break;
						 case  RAWKEY_DELETE:    mplayer_put_key(KEY_DELETE); break;
						 case  RAWKEY_INSERT:    mplayer_put_key(KEY_INSERT); break;
						 case  RAWKEY_HOME:      mplayer_put_key(KEY_HOME); break;
						 case  RAWKEY_END:       mplayer_put_key(KEY_END); break;
						 case  RAWKEY_KP_ENTER:  mplayer_put_key(KEY_KPENTER); break;
						 case  RAWKEY_KP_1:      mplayer_put_key(KEY_KP1); break;
						 case  RAWKEY_KP_2:      mplayer_put_key(KEY_KP2); break;
						 case  RAWKEY_KP_3:      mplayer_put_key(KEY_KP3); break;
						 case  RAWKEY_KP_4:      mplayer_put_key(KEY_KP4); break;
						 case  RAWKEY_KP_5:      mplayer_put_key(KEY_KP5); break;
						 case  RAWKEY_KP_6:      mplayer_put_key(KEY_KP6); break;
						 case  RAWKEY_KP_7:      mplayer_put_key(KEY_KP7); break;
						 case  RAWKEY_KP_8:      mplayer_put_key(KEY_KP8); break;
						 case  RAWKEY_KP_9:      mplayer_put_key(KEY_KP9); break;
						 case  RAWKEY_KP_0:      mplayer_put_key(KEY_KP0); break;

						 case  RAWKEY_LAMIGA:
						 case  RAWKEY_RAMIGA:    break;
					   
						 default:
						 {
							struct InputEvent ie;
							TEXT c;

							ie.ie_Class        = IECLASS_RAWKEY;
							ie.ie_SubClass     = 0;
							ie.ie_Code         = Code;
							ie.ie_Qualifier    = Qualifier;
							ie.ie_EventAddress = NULL;

							if (MapRawKey(&ie, &c, 1, NULL) == 1)
							{
								mplayer_put_key(c);
							}
						 }
				    }
				    break;
		    }
	    }
   }
   else if(SetSignal(0L, appwinsig ) & appwinsig) // Handle Dropped files
   {
	    struct AppMessage *amsg;
	    struct WBArg   *argptr;

		while ((amsg = (struct AppMessage *) GetMsg(awport)))
		{
			LONG i;

			argptr = amsg->am_ArgList;

			for (i = 0; i < amsg->am_NumArgs; i++)
			{
				TEXT path[512];

				NameFromLock(argptr->wa_Lock, path, sizeof(path));

				AddPart(path, argptr->wa_Name, sizeof(path));

				#ifdef CONFIG_GUI
				if (use_gui)
				{
					guiGetEvent(guiLoadFile, path);
					break; // let's just handle first file
				}
				else
				#endif
				{
					TEXT line[512];
					mp_cmd_t * cmd;

					snprintf(line, sizeof(line), "loadfile \"%s\"", path);

					cmd	= mp_input_parse_cmd(line);

					if(cmd)
					{
						if(mp_input_queue_cmd(cmd) == 0)
						{
							mp_cmd_free(cmd);
						}
					}
				}

				argptr++;
			}
			ReplyMsg((struct Message *) amsg);
		}	
   }

   MutexRelease(window_mx);

   return retval;
}

/****************/
static BOOL ismouseon(struct Window *window)
{
	if ( ( (window->MouseX >= 0) && (window->MouseX < window->Width) ) &&
	( (window->MouseY >= 0) && (window->MouseY < window->Height) ) ) return TRUE;

	return FALSE;
}

#ifndef __amigaos4__
static void MyTranspHook(struct Hook *hook,struct Window *window,struct TransparencyMessage *msg)
{
	struct Rectangle rect;

	/* Do not hide border if the pointer is inside and the window is activate */
	if ( ismouseon(window) && (window->Flags & WFLG_WINDOWACTIVE) ) return;

	/* make top border transparent */
	rect.MinX = 0;
	rect.MinY = 0;
	rect.MaxX = window->Width - 1;
	rect.MaxY = window->BorderTop - 1;

	OrRectRegion(msg->Region,&rect);

	/* left border */
	rect.MinX = 0;
	rect.MinY = window->BorderTop;
	rect.MaxX = window->BorderLeft - 1;
	rect.MaxY = window->Height - window->BorderBottom - 1;

	OrRectRegion(msg->Region,&rect);

	/* right border */
	rect.MinX = window->Width - window->BorderRight;
	rect.MinY = window->BorderTop;
	rect.MaxX = window->Width - 1;
	rect.MaxY = window->Height - window->BorderBottom - 1;

	OrRectRegion(msg->Region,&rect);

	/* bottom border */
	rect.MinX = 0;
	rect.MinY = window->Height - window->BorderBottom;
	rect.MaxX = window->Width - 1;
	rect.MaxY = window->Height - 1;

	OrRectRegion(msg->Region,&rect);
}
#endif

static int blanker_count = 0; /* not too useful, but it should be 0 at the end */

void gfx_ControlBlanker(struct Screen * screen, ULONG enable)
{
	struct Library * ibase = (struct Library *) IntuitionBase;
	if(ibase)
	{
		if(ibase->lib_Version > 50 || (ibase->lib_Version == 50 && ibase->lib_Revision >=61))
		{
			if(enable)
			{
				blanker_count++;
			}
			else
			{
				blanker_count--;
			}

			if(!screen)
			{
				screen = LockPubScreen (PubScreenName[0] ? PubScreenName : NULL);
				UnlockPubScreen(NULL, screen);
			}

			mp_msg(MSGT_VO, MSGL_INFO, "VO: %s blanker\n", enable ? "Enabling" : "Disabling" );
			
			if(screen)
			{
//#ifndef __amigaos4__
//				SetAttrs(screen, SA_StopBlanker, enable ? FALSE : TRUE, TAG_DONE);
//#endif
			}
		}
	}
}

/* Just here for debug purpose */
void gfx_BlankerState(void)
{
//	  kprintf("blanker_count = %d\n", blanker_count);
}

void gfx_ShowMouse(struct Screen * screen, struct Window * window, ULONG enable)
{
	struct Library * ibase = (struct Library *) IntuitionBase;
	if(ibase)
	{
/*
#if !PUBLIC_SCREEN
		if(ibase->lib_Version > 50 || (ibase->lib_Version == 50 && ibase->lib_Revision >=61))
		{
			if(screen)
			{
				SetAttrs(screen, SA_ShowPointer, enable, TAG_DONE);
			}

			mouse_hidden = !enable;
		}
		else
#endif
*/
		{
			if(enable)
			{
				ClearPointer(window);			 
			}
			else if(EmptyPointer)
			{
				SetPointer(window, EmptyPointer, 1, 16, 0, 0);
			}
		
			mouse_hidden = !enable;
		}
	}
}


void gfx_get_max_mode(int depth_bits, ULONG *mw,ULONG *mh)
{
	struct List *ml;
	struct P96Mode	*mn;
	ULONG w,h;
	ULONG a,ma = 0;

	if(ml=p96AllocModeListTags(	P96MA_MinDepth, depth_bits, P96MA_MaxDepth, depth_bits,  TAG_DONE))
	{
		for(mn=(struct P96Mode *)(ml->lh_Head);mn->Node.ln_Succ;mn=(struct P96Mode *)mn->Node.ln_Succ)
		{
				w =  p96GetModeIDAttr( mn -> DisplayID, P96IDA_WIDTH );
				h =  p96GetModeIDAttr( mn -> DisplayID, P96IDA_HEIGHT );
				a = w * h;
				if (a >ma) { ma = a; *mw = w; *mh = h; }
		}
		p96FreeModeList(ml);
	}
}
