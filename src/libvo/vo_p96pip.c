/*
 *  vo_p96_pip.c
 *  VO module for MPlayer AmigaOS4
 *  using p96 PIP
 *  Writen by Jörg strohmayer
*/

#undef __USE_INLINE__

#define USE_VMEM64	1

#define ALL_REACTION_CLASSES
#define ALL_REACTION_MACROS

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

//#include <inttypes.h>     // Fix <postproc/rgb2rgb.h> bug
//#include <postproc/rgb2rgb.h>
#include <version.h>

/* ARexx */
#include <proto/intuition.h>
#include "MPlayer-arexx.h"
/* ARexx */

// OS specific
#include <libraries/keymap.h>
#include <osdep/keycodes.h>

#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <proto/wb.h>
#include <proto/icon.h>
#include <proto/requester.h>
#include <proto/layout.h>
#include <proto/utility.h>
#include <gadgets/layout.h>
#define __NOLIBBASE__
#define __NOGLOBALIFACE__
#include <proto/application.h>
#include <proto/gadtools.h>
#include <proto/layers.h>
#include <proto/Picasso96API.h>
#include <exec/types.h>
#include <intuition/icclass.h>
#include <graphics/text.h>
#include <graphics/rpattr.h>
#include <graphics/layers.h>
#include <dos/dostags.h>
#include <dos/dos.h>
#include <classes/requester.h>
#include <intuition/gadgetclass.h>

#include "aspect.h"
#include "input/input.h"
#include "mp_msg.h"
#include "../libmpdemux/demuxer.h"

#include "vo_p96pip.h"

#include <assert.h>
#define CONFIG_GUI 1

#ifdef CONFIG_GUI
#include "vo_p96pip_gui.c"
#endif
#include "vo_p96pip_stuff.c"

#define MIN_WIDTH 290
#define MIN_HEIGHT 217

extern int player_idle_mode;

/***************************/
static BOOL p96_GiveArg(const char *arg)
{
    // Default settings
    p96_BorderMode  = ALWAYSBORDER;

    if ( (arg) )
    {
        if (!(memcmp(arg, "NOBORDER", strlen("NOBORDER") ) ) ) p96_BorderMode = NOBORDER;
        else if (!(memcmp(arg, "TINYBORDER", strlen("TINYBORDER") ) ) ) p96_BorderMode = TINYBORDER;
    }

    return TRUE;
}

/***************************/
static BOOL p96_OpenOSLibs(void)
{
    if (!(P96Base = IExec->OpenLibrary(P96NAME, 2)))
    {
        p96_CloseOSLibs();
        return FALSE;
    }

    if (!(IP96 = (struct P96IFace*) IExec->GetInterface(P96Base, "main", 1, NULL)))
    {
        p96_CloseOSLibs();
        return FALSE;
    }

    if ( ! (LayersBase = IExec->OpenLibrary("layers.library", 0L) ) )
    {
        p96_CloseOSLibs();
        return FALSE;
    }

    ILayers = (struct LayersIFace*) IExec->GetInterface( (struct Library*)LayersBase, "main", 1, NULL );
    if (!ILayers)
    {
        p96_CloseOSLibs();
        return FALSE;
    }

    if ( ! (GadToolsBase = (struct Library *) IExec->OpenLibrary("gadtools.library",0UL)) )
    {
        p96_CloseOSLibs();
        return FALSE;
    }

    IGadTools = (struct GadToolsIFace *) IExec->GetInterface(GadToolsBase,"main",1,0UL);
    if (!IGadTools)
    {
        p96_CloseOSLibs();
        return FALSE;
    }

	if ( ! (ApplicationBase = (struct Library *) IExec->OpenLibrary("application.library", 51L)) )
	{
        p96_CloseOSLibs();
		return FALSE;
	}
	else
	{
	    if (ApplicationBase->lib_Version > 53 || (ApplicationBase->lib_Version >= 53 && ApplicationBase->lib_Revision >= 4))
    	    hasNotificationSystem = TRUE;
	    else
    	    hasNotificationSystem = FALSE;
	}
	IApplication = (struct ApplicationIFace *) IExec->GetInterface(ApplicationBase,"application",1,NULL);
	if (!IApplication)
	{
        p96_CloseOSLibs();
		return FALSE;
	}

    return TRUE;
}

/**********************/
static void p96_CloseOSLibs(void)
{
    if (IP96)
    {
        IExec->DropInterface((struct Interface *)IP96);
        IP96 = NULL;
    }

    if (P96Base)
    {
        IExec->CloseLibrary (P96Base);
        P96Base = NULL;
    }

    if (ILayers)
    {
        IExec->DropInterface((struct Interface*) ILayers);
    }

    if (LayersBase)
    {
        IExec->CloseLibrary(LayersBase);
        LayersBase = NULL;
    }

    if (IGadTools)
    {
        IExec->DropInterface((struct Interface*) IGadTools);
    }

    if (GadToolsBase)
    {
        IExec->CloseLibrary(GadToolsBase);
        GadToolsBase = NULL;
    }

	if (ApplicationBase)
	{
		IExec->CloseLibrary(ApplicationBase);
		ApplicationBase = NULL;
	}
	if (IApplication)
	{
		IExec->DropInterface((struct Interface*)IApplication);
		IApplication = NULL;
    }

}

/***************************************************/
static BOOL p96_CheckEvents(struct Window *My_Window, uint32_t *window_height, uint32_t *window_width,  uint32_t *window_left, uint32_t *window_top )
{
    ULONG retval = FALSE;
    //static BOOL mouseonborder;
    ULONG info_sig, ApplSig;
    ULONG menucode;
	ULONG current_time = rxid_get_time_pos();

	if (vo_fs && !mouse_hidden)
	{
		if (!p_secs1 && !p_mics1)
			IIntuition->CurrentTime(&p_secs1, &p_mics1);
		else
		{
			IIntuition->CurrentTime(&p_secs2, &p_mics2);
			if (p_secs2-p_secs1>=5) //five seconds
			{
				mouse_hidden = TRUE;
				IIntuition->SetPointer(My_Window, EmptyPointer, 1, 16, 0, 0);
				p_secs1 = p_secs2 = p_mics1 = p_mics2=0;
			}
		}
	}
	IApplication->GetApplicationAttrs(AppID, APPATTR_Port, (ULONG)&NotSerAppPort, TAG_DONE);
	ApplSig = (1L << NotSerAppPort->mp_SigBit);
    if(IExec->SetSignal(0L, ApplSig ) & ApplSig)
    {
    	struct ApplicationMsg *ApplMsg;
		while ((ApplMsg = (struct ApplicationMsg *)IExec->GetMsg(NotSerAppPort)))
		    IExec->ReplyMsg((struct Message *)ApplMsg);
    }

	if (hasGUI == TRUE)
	{
	    if(IExec->SetSignal(0L,_timerSig ) & _timerSig)
		{
			if (reset_time>0)
				IIntuition->RefreshSetGadgetAttrs((struct Gadget*) OBJ[GID_Time], My_Window, NULL, SCROLLER_Top, current_time, TAG_DONE);
			TimerReset(RESET_TIME);
		}
	}

    info_sig=1L<<(UserMsg)->mp_SigBit;

    if(IExec->SetSignal(0L,info_sig ) & info_sig)
    {
		struct IntuiMessage * IntuiMsg;
		ULONG Class;
		UWORD Code;
		char *file;
		CONST_STRPTR errmsg = "Error during requester opening!";

        while ( ( IntuiMsg = (struct IntuiMessage *) IExec->GetMsg( UserMsg ) ) )
        {
            Class = IntuiMsg->Class;
            Code = IntuiMsg->Code;
            IExec->ReplyMsg( (struct Message *) IntuiMsg);

            switch (Class)
            {
                case IDCMP_MENUPICK:
                    menucode = IntuiMsg->Code;
                    if (menucode != MENUNULL)
                    {
                        while (menucode != MENUNULL)
                        {
                            struct MenuItem  *item = IIntuition->ItemAddress(menu,menucode);

                            //Printf("Menu %ld, item %ld, sub-item %2ld (item address %08lX)\n",
                            //MENUNUM(menucode),
                            //ITEMNUM(menucode),
                            //SUBNUM(menucode),
                            //item);

                            switch (MENUNUM(menucode))
                            {
                                case 0:  /* First menu */

                                    switch (ITEMNUM(menucode))
                                    {
                                        case 0:  /* Load File */
											if (choosing == 0)
											{
	                                        	file = LoadFile(NULL);

    	                                    	if (file)
    	                                    	{
    	                                    		#ifdef CONFIG_GUI
													guiGetEvent(guiCEvent, (void *)guiSetPlay);
													#endif
													NotifyServer(file);
													PlayFile((char*)file);
    	                                    	}
    	                                    }
                                        break;
                                        case 1:  /* Open DVD */
                                        {
											struct Process *DvdMessage;
											if (choosing == 0)
											{
												DvdMessage = (struct Process *) IDOS->CreateNewProcTags (
		   													NP_Entry, 		(ULONG) OpenDVD,
						   									NP_Name,	   	"Load DVD",
															NP_StackSize,   262144,
															NP_Child,		TRUE,
															NP_Priority,	0,
								   							NP_ExitData, 	IDOS,
						  									TAG_DONE);
												if (!DvdMessage)
													PrintMsg(errmsg,REQTYPE_INFO,REQIMAGE_ERROR);
											}
										}
										break;
										case 2:
										{
											struct Process *VcdMessage;
											if (choosing == 0)
											{
												VcdMessage = (struct Process *) IDOS->CreateNewProcTags (
		   													NP_Entry, 		(ULONG) OpenVCD,
						   									NP_Name,	   	"Load VCD",
															NP_StackSize,   262144,
															NP_Child,		TRUE,
															NP_Priority,	0,
								   							NP_ExitData, 	IDOS,
						  									TAG_DONE);
												if (!VcdMessage)
													PrintMsg(errmsg,REQTYPE_INFO,REQIMAGE_ERROR);
											}
										}
										break;
										case 3:
										{
											struct Process *NetworkMessage;
											if (choosing == 0)
											{
												NetworkMessage = (struct Process *) IDOS->CreateNewProcTags (
		   													NP_Entry, 		(ULONG) OpenNetwork,
						   									NP_Name,	   	"Load Network",
															NP_StackSize,   262144,
															NP_Child,		TRUE,
															NP_Priority,	0,
								   							NP_ExitData, 	IDOS,
						  									TAG_DONE);
												if (!NetworkMessage)
													PrintMsg(errmsg,REQTYPE_INFO,REQIMAGE_ERROR);
											}
										}
										break;
										case 5:
											ShowAbout();
										break;
                                        case 6:  /* Quit */
                                            put_command0(MP_CMD_QUIT);
                                        break;
                                    }
                                break;

                                case 1:
                                    switch (ITEMNUM(menucode))
                                    {
                                        case 0:  /* pause/play */
											if (isPaused == TRUE)
												isPaused = FALSE;
											else
												isPaused = TRUE;

											ChangePauseButton();
                                            put_command0(MP_CMD_PAUSE);
                                        break;

                                        case 1:  /* Quit */
                                            put_command0(MP_CMD_PAUSE);
											isPaused = TRUE;
											ChangePauseButton();
											isPaused = FALSE;
                                            //put_command0(MP_CMD_STOP);
                                        break;
                                        case 2:  /* Loop */
                                        	if (loop_on==0)
                                        	{
                                        		loop_on = 1;
	                                        	put_icommand1(MP_CMD_LOOP,1);
                                        	}
                                        	else
                                        	{
                                        		loop_on = 0;
	                                        	put_icommand1(MP_CMD_LOOP,-1);
                                        	}
                                        break;
                                    }
                                break;
                                case 2:
                                    switch (ITEMNUM(menucode))
                                    {
                                        case 0:
                                            put_command0(MP_CMD_VO_ONTOP);
                                        break;
                                        case 1:
                                            put_command0(MP_CMD_SUB_SELECT);
                                            //mplayer_put_key('o'); /* This must be changed to use MPlayer command */
                                        break;
                                        case 2:
											p_secs1 = p_secs2 = p_mics2 = p_mics1 = 0;
											mouse_hidden=FALSE;
	                                        put_command0(MP_CMD_VO_FULLSCREEN);
                                        break;
                                        case 3:
	                                        put_command0(MP_CMD_SCREENSHOT);
                                        break;
                                        case 5:
	                                        ChangeWindowSize(x1);
                                        break;
                                        case 6:
	                                        ChangeWindowSize(x2);
                                        break;
                                        case 7:
	                                        ChangeWindowSize(xF);
                                        break;
                                        case 8:
	                                        ChangeWindowSize(xH);
                                        break;
                                        case 10:
	                                        switch (SUBNUM(menucode))
    	                                    {
    	                                    	case 0:
		    	                                    AspectRatio = AR_ORIGINAL;
	                        	    	            ChangeWindowSize(AR_ORIGINAL);
                                	    	    break;
                                	    	    case 2:
		    	                                    AspectRatio = AR_16_10;
	                        	    	            ChangeWindowSize(AR_16_10);
    	    	                                case 3:
		    	                                    AspectRatio = AR_16_9;
	    	    	                                ChangeWindowSize(AR_16_9);
            	    	                        break;
                                    	    	case 4:
		    	                                    AspectRatio = AR_185_1;
		                                    	    ChangeWindowSize(AR_185_1);
		                                        break;
    	    	                                case 5:
		    	                                    AspectRatio = AR_221_1;
	    	    	                                ChangeWindowSize(AR_221_1);
            	    	                        break;
    	    	                                case 6:
		    	                                    AspectRatio = AR_235_1;
	    	    	                                ChangeWindowSize(AR_235_1);
            	    	                        break;
    	    	                                case 7:
		    	                                    AspectRatio = AR_239_1;
	    	    	                                ChangeWindowSize(AR_239_1);
            	    	                        break;
    	    	                                case 8:
		    	                                    AspectRatio = AR_5_3;
	    	    	                                ChangeWindowSize(AR_5_3);
            	    	                        break;
    	    	                                case 9:
		    	                                    AspectRatio = AR_4_3;
	    	    	                                ChangeWindowSize(AR_4_3);
            	    	                        break;
    	    	                                case 10:
		    	                                    AspectRatio = AR_5_4;
	    	    	                                ChangeWindowSize(AR_5_4);
            	    	                        break;
    	    	                                case 11:
		    	                                    AspectRatio = AR_1_1;
	    	    	                                ChangeWindowSize(AR_1_1);
            	    	                        break;
                	    	                }
                	    	            break;
                	                }
                                break;

                                case 3:
                                    switch (ITEMNUM(menucode))
                                    {
                                        case 0:
						                    mp_input_queue_cmd(mp_input_parse_cmd("mute"));
        								break;
        								case 1:
		                                    SetVolume(VOL_UP);
		                                break;
		                                case 2:
		                                    SetVolume(VOL_DOWN);
										break;
									}
                                break;
                            }

                            /* Handle multiple selection */
                            menucode = item->NextSelect;
                        }
                    }
                break;
                case IDCMP_MOUSEMOVE:
                {
					WORD X_coord, Y_coord;
					X_coord = IntuiMsg->MouseX - My_Window->BorderLeft;
					Y_coord = IntuiMsg->MouseY - My_Window->BorderTop;

   					if (vo_fs)
					{
						if (mouse_hidden)
						{
							mouse_hidden=FALSE;
							IIntuition->ClearPointer(My_Window);
						}
					}
				}
				break;
				case IDCMP_MOUSEBUTTONS:
					if (vo_fs)
					{
						if (mouse_hidden)
						{
							mouse_hidden=FALSE;
							IIntuition->ClearPointer(My_Window);
						}
					}
					switch (Code)
					{
						case SELECTDOWN:
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
							if ( IIntuition->DoubleClick(secs, mics, secs2, mics2) )
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
					}
				break;

                case IDCMP_CLOSEWINDOW:
	                player_idle_mode = 0;
                    mplayer_put_key(KEY_ESC);
                break;

                case IDCMP_VANILLAKEY:
                    switch ( Code )
                    {
						case 'f':
						case 'F':
                            put_command0(MP_CMD_VO_FULLSCREEN);
							p_secs1 = p_secs2 = p_mics2 = p_mics1 = 0;
							mouse_hidden=FALSE;
						break;
                        case  '\e':
	                        player_idle_mode = 0;
                            mplayer_put_key(KEY_ESC);
                        break;
						case 'p':
						case 'P':
							if (isPaused == TRUE)
								isPaused = FALSE;
							else
								isPaused = TRUE;

							ChangePauseButton();
							put_command0(MP_CMD_PAUSE);
						break;
                        default:
                            if (Code!='\0') mplayer_put_key(Code); break;
                    }
                break;

                case IDCMP_RAWKEY:
                    switch ( Code )
                    {
                        case  RAWKEY_PAGEDOWN:
                            mplayer_put_key(KEY_PGDWN);
                        break;
                        case  RAWKEY_PAGEUP:
                            mplayer_put_key(KEY_PGUP);
                        break;
                        case  RAWKEY_CRSRRIGHT:
                            mplayer_put_key(KEY_RIGHT);
                        break;
                        case  RAWKEY_CRSRLEFT:
                            mplayer_put_key(KEY_LEFT);
                        break;
                        case  RAWKEY_CRSRUP:
                            mplayer_put_key(KEY_UP);
                        break;
                        case  RAWKEY_CRSRDOWN:
                            mplayer_put_key(KEY_DOWN);
                        break;
                    }
                break;
				case IDCMP_CHANGEWINDOW:
				{
					IIntuition->GetWindowAttr(My_Window, WA_Left,  &old_d_posx,  sizeof(old_d_posx));
					IIntuition->GetWindowAttr(My_Window, WA_Top, &old_d_posy, sizeof(old_d_posy));
				}
				break;
                case IDCMP_NEWSIZE:
                {
					IIntuition->GetWindowAttr(My_Window, WA_InnerWidth,  &old_d_width,  sizeof(old_d_width));
					IIntuition->GetWindowAttr(My_Window, WA_InnerHeight, &old_d_height, sizeof(old_d_height));
					IIntuition->GetWindowAttr(My_Window, WA_Left,  &old_d_posx,  sizeof(old_d_posx));
					IIntuition->GetWindowAttr(My_Window, WA_Top, &old_d_posy, sizeof(old_d_posy));

					//IIntuition->RefreshGList((struct Gadget*)WinLayout, My_Window, NULL, -1);
				}
	            break;
				case IDCMP_IDCMPUPDATE:
				{
                    struct TagItem *tags = (struct TagItem *)IntuiMsg->IAddress;

					if (IUtility->FindTagItem(LAYOUT_RelVerify, tags))
                    {
                        ULONG gid = IUtility->GetTagData(GA_ID, 0, tags);
						switch (gid)
						{
							case GID_Back:
	                            mplayer_put_key(KEY_LEFT);
							break;
							case GID_Forward:
	                            mplayer_put_key(KEY_RIGHT);
							break;
							case GID_Pause:
							{
								if (isPaused == TRUE)
									isPaused = FALSE;
								else
									isPaused = TRUE;

								ChangePauseButton();
								if (!Stopped)
									put_command0(MP_CMD_PAUSE);
								else
								{
									if (current_filename)
										PlayFile(current_filename);
								}
							}
							break;
							case GID_Stop:
							{
								mp_cmd_t * cmd = malloc(sizeof(mp_cmd_t));

								if(cmd)
								{
									ULONG iValue = 0;
									int abs = 2;

									IIntuition->GetAttr(SCROLLER_Total, OBJ[GID_Time], &iValue);
									cmd->pausing = 0;
									cmd->id    = MP_CMD_SEEK;
									cmd->name  = strdup("seek");
									cmd->nargs = 2;
									cmd->args[0].type = MP_CMD_ARG_FLOAT;
									cmd->args[0].v.f=iValue/1000.0f;
									cmd->args[1].type = MP_CMD_ARG_INT;
									cmd->args[1].v.i=abs;
									cmd->args[2].type = -1;
									cmd->args[2].v.i=0;

									if(mp_input_queue_cmd(cmd) == 0)
									{
										mp_cmd_free(cmd);
									}
								}

								isPaused = TRUE;
								ChangePauseButton();
								isPaused = FALSE;
							}
							break;
							case GID_Eject:
								if (choosing == 0)
								{
                                    file = LoadFile(NULL);

    	                            if (file)
    	                            {
    	                            	#ifdef CONFIG_GUI
										guiGetEvent(guiCEvent, (void *)guiSetPlay);
										#endif
										NotifyServer(file);
										PlayFile((char*)file);
    	                            }
    	                        }
							break;
							case GID_FullScreen:
								p_secs1 = p_secs2 = p_mics2 = p_mics1 = 0;
								mouse_hidden = FALSE;
								put_command0(MP_CMD_VO_FULLSCREEN);
							break;
							case GID_Time:
							{
								mp_cmd_t * cmd = malloc(sizeof(mp_cmd_t));

								if(cmd)
								{
									ULONG iValue = 0;
									int abs = 2;

									IIntuition->GetAttr(SCROLLER_Top, OBJ[GID_Time], &iValue);
									cmd->pausing = 0;
									cmd->id    = MP_CMD_SEEK;
									cmd->name  = strdup("seek");
									cmd->nargs = 2;
									cmd->args[0].type = MP_CMD_ARG_FLOAT;
									cmd->args[0].v.f=iValue/1000.0f;
									cmd->args[1].type = MP_CMD_ARG_INT;
									cmd->args[1].v.i=abs;
									cmd->args[2].type = -1;
									cmd->args[2].v.i=0;

									if(mp_input_queue_cmd(cmd) == 0)
									{
										mp_cmd_free(cmd);
									}
								}
							}
							break;
							case GID_Volume:
							{
								ULONG iValue = 0;
								IIntuition->GetAttr(SCROLLER_Top, OBJ[GID_Volume], &iValue);
								put_fcommand2(MP_CMD_VOLUME,(float)(iValue/1.20),1);
							}
							break;
						}
                    }
                }
	            break;
	            case IDCMP_GADGETDOWN:
	            {
	            	ULONG GadgetPressed = ((struct Gadget *)IntuiMsg->IAddress)->GadgetID;
	            	switch (GadgetPressed)
	            	{
						case GID_Iconify:
						{
							MPLAppIcon = IWorkbench->AddAppIcon(0, (uint32)My_Window, "MPlayer", AppPort,  0, DIcon, TAG_DONE);
							IExec->ReplyMsg(&IntuiMsg->ExecMessage);
							IIntuition->HideWindow(My_Window);
						}
						break;
						case GID_Time:
						{
			            	reset_time = 0;
		    	        	TimerReset(RESET_TIME);
		    	        }
		    	        break;
					}
				}
	            break;
	            case IDCMP_GADGETUP:
	            {
	            	ULONG GadgetPressed = ((struct Gadget *)IntuiMsg->IAddress)->GadgetID;
	            	switch (GadgetPressed)
	            	{
						case GID_Time:
						{
							reset_time = RESET_TIME;
			            	TimerReset(RESET_TIME);
			            }
					}
				}
	            break;
            }
        }
    }

    appw_events();

    return retval;
}

/***************
static BOOL ismouseon(struct Window *window)
{
    if ( ( (window->MouseX >= 0) && (window->MouseX < window->Width) ) &&
    ( (window->MouseY >= 0) && (window->MouseY < window->Height) ) ) return TRUE;

    return FALSE;
}
*/
static char *p96_GetWindowTitle(void)
{
    if (filename)
    {
    	current_filename = strdup(filename);
        window_title = (char *) IExec->AllocVec(strlen(filename) + 11,MEMF_SHARED|MEMF_CLEAR);
        strcpy(window_title,"MPlayer - ");
        strcat(window_title,filename);
    }
    else
    {
        window_title = (char *) IExec->AllocVec(strlen("MPlayer")+1, MEMF_SHARED|MEMF_CLEAR);
        strcpy(window_title,"MPlayer");
    }
    return(window_title);
}

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
    int numbuffers=0;

	if (!w || !h)
		return;

    IP96->p96PIP_GetTags(My_Window, P96PIP_SourceBitMap   , (ULONG)&bm   ,
    								P96PIP_NumberOfBuffers, &numbuffers  ,
    								TAG_END, TAG_DONE );

    if (! (lock_h = IP96->p96LockBitMap(bm, (uint8 *)&ri, sizeof(struct RenderInfo))))
    {
        // Unable to lock the BitMap -> do nothing
        return;
    }

    dstbase = (UWORD *)(ri.Memory + ( ( (y0 * image_width) + x0) * 2));

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
        dstbase +=   image_width;

    } while (--h);

	if (numbuffers>1)
	{
		uint32 WorkBuf, DispBuf, NextWorkBuf;

		IP96->p96PIP_GetTags(My_Window,	P96PIP_WorkBuffer,      &WorkBuf,
								 		P96PIP_DisplayedBuffer, &DispBuf,
								 		TAG_END, TAG_DONE );

		NextWorkBuf = (WorkBuf+1) % numbuffers;

		if (NextWorkBuf == DispBuf) IGraphics->WaitTOF();

		IP96->p96PIP_SetTags(My_Window,	P96PIP_VisibleBuffer, WorkBuf, //P96PIP_WorkBuffer, WorkBuf,
								 		P96PIP_WorkBuffer,    NextWorkBuf, //P96PIP_DisplayedBuffer, NextWorkBuf,
								 		TAG_END, TAG_DONE);
	}

    IP96->p96UnlockBitMap(bm, lock_h);
}

/******************************** PREINIT ******************************************/
static int preinit(const char *arg)
{
    mp_msg(MSGT_VO, MSGL_INFO, "VO: [p96_pip]\n");
    if( !p96_OpenOSLibs() )
    {
        return -1;
    }

    if (!p96_GiveArg(arg))
    {
        p96_CloseOSLibs();
        return -1;
    }

#ifdef CONFIG_GUI
	mygui = malloc(sizeof(*mygui));
	initguimembers();
#endif
    return 0;
}

static ULONG GoFullScreen(void)
{
	uint32_t left = 0, top = 0;
    ULONG ModeID = INVALID_ID;

	int i = 0;
	if (WinLayout)
		IIntuition->DisposeObject(WinLayout);

	for (i=0;i<6;i++)
		if (NULL != image[i]) IIntuition->DisposeObject((Object *) image[i]);

    if (My_Window)
    {
		IP96->p96PIP_Close(My_Window);
		if (IconifyGadget)
		{
			IIntuition->DisposeObject(IconifyGadget);
			IconifyGadget = NULL;
		}
		if (IconifyImage)
		{
			IIntuition->DisposeObject(IconifyImage);
			IconifyImage = NULL;
		}
		My_Window = NULL;
	}

	My_Screen = IIntuition->OpenScreenTags ( 		NULL,
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
	IApplication->SetApplicationAttrs(AppID, APPATTR_NeedsGameMode, TRUE, TAG_DONE);

	/* Fill the screen with black color */
	IP96->p96RectFill(&(My_Screen->RastPort), 0,0, My_Screen->Width, My_Screen->Height, 0x00000000);

	/* calculate new video size/aspect */
	aspect_save_screenres(My_Screen->Width,My_Screen->Height);

   	aspect(&out_width,&out_height,A_ZOOM);

    left=(My_Screen->Width-out_width)/2;
    top=(My_Screen->Height-out_height)/2;

    My_Window = IP96->p96PIP_OpenTags(
                      	P96PIP_SourceFormat, RGBFB_YUV422CGX,
                      	P96PIP_SourceWidth,  image_width,
                      	P96PIP_SourceHeight, image_height,
						P96PIP_NumberOfBuffers, NUMBUFFERS,

                      	WA_CustomScreen,    (ULONG) My_Screen,
                      	WA_ScreenTitle,     (ULONG) "MPlayer ",
                      	WA_Left,            left,
                      	WA_Top,             top,
                      	WA_SmartRefresh,    TRUE,
                      	WA_CloseGadget,     FALSE,
                      	WA_DepthGadget,     FALSE,
                      	WA_DragBar,         FALSE,
                      	WA_Borderless,      TRUE,
                      	WA_SizeGadget,      FALSE,
                      	WA_Activate,        TRUE,
                      	WA_StayTop,		  	TRUE,
						WA_DropShadows,		FALSE,
                      	WA_Flags,			WFLG_RMBTRAP,
                      	WA_IDCMP,           IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_VANILLAKEY | IDCMP_MOUSEMOVE,
                      	TAG_DONE);

	hasGUI = FALSE;

	if ( !My_Window )
	{
		return INVALID_ID;
	}
	IP96->p96RectFill(My_Window->RPort, 0,0, My_Screen->Width, My_Screen->Height, 0x00000000);

	mouse_hidden = TRUE;
	IIntuition->SetPointer(My_Window, EmptyPointer, 1, 16, 0, 0);

    if ( (ModeID = IGraphics->GetVPModeID(&My_Window->WScreen->ViewPort) ) == INVALID_ID)
   	{
        return INVALID_ID;
   	}

    IIntuition->SetWindowAttrs(My_Window,
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

    IIntuition->ScreenToFront(My_Screen);

    return ModeID;
}
/**********************/
static ULONG Open_PIPWindow(void)
{
    // Window
    ULONG ModeID = INVALID_ID;
    APTR vi;

    My_Window = NULL;

    if ( ( My_Screen = IIntuition->LockPubScreen ( NULL ) ) )
    {
        struct DrawInfo *dri;

        vi = IGadTools->GetVisualInfoA(My_Screen,NULL);
        if (vi)
        {
            if (isStarted == FALSE)
            {
                isStarted = TRUE;
            }
            else
            {
            	int i = 0;
            	
                if (is_ontop==1)
                    nm[13].nm_Flags = MENUTOGGLE|CHECKIT|CHECKED;
                else
                    nm[13].nm_Flags = MENUTOGGLE|CHECKIT;

                if (loop_on==1)
                    nm[11].nm_Flags = MENUTOGGLE|CHECKIT|CHECKED;
                else
                    nm[11].nm_Flags = MENUTOGGLE|CHECKIT;

				for (i=24;i<35;i++)
                    nm[i].nm_Flags = MENUTOGGLE|CHECKIT;

				switch(AspectRatio)
				{
					case AR_ORIGINAL:
	                    nm[24].nm_Flags = MENUTOGGLE|CHECKIT|CHECKED;
					break;
					case AR_16_10:
	                    nm[25].nm_Flags = MENUTOGGLE|CHECKIT|CHECKED;
					break;
					case AR_16_9:
	                    nm[26].nm_Flags = MENUTOGGLE|CHECKIT|CHECKED;
					break;
					case AR_185_1:
	                    nm[27].nm_Flags = MENUTOGGLE|CHECKIT|CHECKED;
					break;
					case AR_221_1:
	                    nm[28].nm_Flags = MENUTOGGLE|CHECKIT|CHECKED;
					break;
					case AR_235_1:
	                    nm[29].nm_Flags = MENUTOGGLE|CHECKIT|CHECKED;
					break;
					case AR_239_1:
	                    nm[30].nm_Flags = MENUTOGGLE|CHECKIT|CHECKED;
					break;
					case AR_5_3:
	                    nm[31].nm_Flags = MENUTOGGLE|CHECKIT|CHECKED;
					break;
					case AR_4_3:
	                    nm[32].nm_Flags = MENUTOGGLE|CHECKIT|CHECKED;
					break;
					case AR_5_4:
	                    nm[33].nm_Flags = MENUTOGGLE|CHECKIT|CHECKED;
					break;
					case AR_1_1:
	                    nm[34].nm_Flags = MENUTOGGLE|CHECKIT|CHECKED;
					break;
				}
            }

            menu = IGadTools->CreateMenusA(nm,NULL);
            if (menu)
            {
                if (IGadTools->LayoutMenus(menu,vi,GTMN_NewLookMenus,TRUE,TAG_END))
                {
                    ModeID = IGraphics->GetVPModeID(&My_Screen->ViewPort);

                    if ( (dri = IIntuition->GetScreenDrawInfo(My_Screen) ) )
                    {
                        ULONG bw, bh;
					    static TEXT ProgramName[512];
						BOOL HasName = FALSE;
						
						IconifyImage = IIntuition->NewObject(NULL, "sysiclass", SYSIA_DrawInfo, (ULONG) dri, SYSIA_Which, ICONIFYIMAGE, TAG_DONE);

						IconifyGadget = IIntuition->NewObject(NULL, "buttongclass",
							GA_ID, 			GID_Iconify,
							GA_RelVerify, 	TRUE,
							GA_Image, 		IconifyImage,
							GA_TopBorder, 	TRUE,
							GA_RelRight, 	0,
							GA_Titlebar, 	TRUE,
							TAG_END);

						if (IDOS->Cli())
						{
							TEXT tempFileName[1024];

							if (IDOS->GetProgramName(tempFileName, sizeof(tempFileName)))
							{
								strcpy(ProgramName, "PROGDIR:");
								strlcat(ProgramName, IDOS->FilePart(tempFileName), sizeof(ProgramName));
								HasName = TRUE;
							}
						}
						else
						{
							strlcpy(ProgramName, IExec->FindTask(NULL)->tc_Node.ln_Name, sizeof(ProgramName));
							HasName = TRUE;
						}

						DIcon = HasName ? IIcon->GetDiskObject(ProgramName) : NULL;

                        switch(p96_BorderMode)
                        {
                            case NOBORDER:
								bw = 0;
								bh = 0;
							break;

                            default:
								bw = My_Screen->WBorLeft + My_Screen->WBorRight;
								bh = My_Screen->WBorTop + My_Screen->Font->ta_YSize + 1 + My_Screen->WBorBottom;
							break;
                        }

                        if (FirstTime)
                        {
							/* Set dimension and position */

                            win_left = (My_Screen->Width - (window_width + bw)) / 2;
                            win_top  = (My_Screen->Height - (window_height + bh)) / 2;
                            old_d_posx = win_left;
                            old_d_posy = win_top;
                            FirstTime = FALSE;
                        }

                        switch(p96_BorderMode)
                        {
                            case NOBORDER:
                                My_Window = IP96->p96PIP_OpenTags(
                                    P96PIP_SourceFormat, 	RGBFB_YUV422CGX,
                                    P96PIP_SourceWidth,  	image_width,
                                    P96PIP_SourceHeight, 	image_height,
                                    P96PIP_AllowCropping, 	TRUE,
									P96PIP_NumberOfBuffers, NUMBUFFERS,

                                    WA_CustomScreen,    (ULONG) My_Screen,
                                    WA_ScreenTitle,     (ULONG) "MPlayer ",
                                    WA_Left,            old_d_posx,
                                    WA_Top,             old_d_posy,
									WA_InnerWidth,		window_width,
									WA_InnerHeight,		window_height,
                                    WA_NewLookMenus,    TRUE,
                                    WA_SmartRefresh,    TRUE,
                                    WA_CloseGadget,     FALSE,
                                    WA_DepthGadget,     FALSE,
                                    WA_DragBar,         FALSE,
                                    WA_Borderless,      TRUE,
                                    WA_SizeGadget,      FALSE,
									WA_Activate,		TRUE,
									WA_Hidden,			TRUE,
                                    WA_StayTop,         (is_ontop==1) ? TRUE : FALSE,
                                    WA_IDCMP,           IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_VANILLAKEY | IDCMP_CLOSEWINDOW | IDCMP_MENUPICK | IDCMP_MOUSEMOVE | IDCMP_CHANGEWINDOW,
                                    WA_Gadgets,         &MyDragGadget,
                                    WA_MaxWidth, 		My_Screen->Width,
                                    WA_MaxHeight, 		My_Screen->Height,
                                TAG_DONE);
                                hasGUI = FALSE;
                            break;

                            case TINYBORDER:
                                My_Window = IP96->p96PIP_OpenTags(
                                    P96PIP_SourceFormat, 	RGBFB_YUV422CGX,
                                    P96PIP_SourceWidth,  	image_width,
                                    P96PIP_SourceHeight, 	image_height,
                                    P96PIP_AllowCropping, 	TRUE,
									P96PIP_NumberOfBuffers, NUMBUFFERS,

                                    WA_CustomScreen,    (ULONG) My_Screen,
                                    WA_ScreenTitle,     (ULONG) "MPlayer ",
                                    WA_Left,            old_d_posx,
                                    WA_Top,             old_d_posy,
									WA_InnerWidth,		window_width,
									WA_InnerHeight,		window_height,
                                    WA_NewLookMenus,    TRUE,
                                    WA_SmartRefresh,    TRUE,
                                    WA_CloseGadget,     FALSE,
                                    WA_DepthGadget,     FALSE,
                                    WA_DragBar,         FALSE,
                                    WA_Borderless,      FALSE,
                                    WA_SizeGadget,      FALSE,
									WA_Activate,		TRUE,
									WA_Hidden,			TRUE,
                                    WA_StayTop,         (is_ontop==1) ? TRUE : FALSE,
                                    WA_IDCMP,           IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_VANILLAKEY | IDCMP_CLOSEWINDOW | IDCMP_MENUPICK | IDCMP_MOUSEMOVE | IDCMP_CHANGEWINDOW,
                                    WA_Gadgets,         &MyDragGadget,
                                    WA_MaxWidth, 		My_Screen->Width,
                                    WA_MaxHeight, 		My_Screen->Height,
                                TAG_DONE);
                                hasGUI = FALSE;
                            break;

                            default:
								My_Window = IP96->p96PIP_OpenTags(
									P96PIP_SourceFormat, 	RGBFB_YUV422CGX,
									P96PIP_SourceWidth,		image_width,
									P96PIP_SourceHeight,	image_height,
									P96PIP_AllowCropping,	TRUE,
									P96PIP_NumberOfBuffers, NUMBUFFERS,
									P96PIP_Relativity, PIPRel_Width | PIPRel_Height,
									P96PIP_Left, 1,
									P96PIP_Top, 1,
									P96PIP_Width, (ULONG) -1,
									P96PIP_Height, (ULONG)-34,
									P96PIP_ColorKeyPen, 	0,

									WA_CustomScreen,	(ULONG) My_Screen,
									WA_ScreenTitle,		(ULONG) "MPlayer",
									WA_Title,			p96_GetWindowTitle(),
									WA_Left,			old_d_posx,
									WA_Top,				old_d_posy,
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
                                    WA_StayTop,         (is_ontop==1) ? TRUE : FALSE,
									WA_IDCMP,			IDCMP_NEWSIZE |
														IDCMP_MOUSEBUTTONS |
														IDCMP_RAWKEY |
														IDCMP_VANILLAKEY |
														IDCMP_CLOSEWINDOW |
														IDCMP_MENUPICK |
														IDCMP_MOUSEMOVE |
														IDCMP_IDCMPUPDATE |
														IDCMP_CHANGEWINDOW |
														IDCMP_GADGETUP |
														IDCMP_GADGETDOWN,
								TAG_DONE);
                                hasGUI = TRUE;
                        }

                        IIntuition->FreeScreenDrawInfo(My_Screen, dri);
                    }

                    if (My_Window)
                    {
						if (DIcon)
							IIntuition->AddGadget(My_Window, (struct Gadget *)IconifyGadget, -1);

                    	if (hasGUI == TRUE)
                    	{
							WinLayout = (Object*) CreateWinLayout(My_Screen, My_Window);

							if (WinLayout)
							{
									struct IBox *bbox;

									ULONG total_time = rxid_get_time_length();
									OBJ[GID_MAIN] = (Object *)ToolBarObject;

									IIntuition->AddGadget(My_Window, (struct Gadget*)WinLayout, (uint16)~0);
									IIntuition->RefreshSetGadgetAttrs((struct Gadget*) OBJ[GID_Time], My_Window, NULL, SCROLLER_Total, total_time, TAG_DONE);

									IIntuition->RefreshGList((struct Gadget*)WinLayout, My_Window, NULL, -1);

									IIntuition->GetAttr(SPACE_AreaBox,OBJ[GID_MAIN],(ULONG *)&bbox);
									//IP96->p96RectFill(My_Window->RPort,bbox->Left,bbox->Top,bbox->Width-1,bbox->Height-1,0xffffffff);
	 	                    }
						}

						/* Exit from Game Mode */
						IApplication->SetApplicationAttrs(AppID, APPATTR_NeedsGameMode, FALSE, TAG_DONE);

						/* Clearing Pointer */
						IIntuition->ClearPointer(My_Window);

						/* Adding the menu */
                       	IIntuition->SetMenuStrip(My_Window, menu);

						/* Recreate the AppWindow */
						if (AppPort)
						{
							AppWin = IWorkbench->AddAppWindow (0, 0, My_Window, AppPort, NULL);
							if (!AppWin)
							{
	     						mp_msg(MSGT_VO, MSGL_INFO, "Cannot create AppWindow!\n");
							}
						}
                    }

					vo_screenwidth = My_Screen->Width;
					vo_screenheight = My_Screen->Height;
                }
            }
        }

		IIntuition->UnlockPubScreen(NULL, My_Screen);
    }

    if ( !My_Window )
    {
        mp_msg(MSGT_VO, MSGL_ERR, "Unable to open a window\n");
        return INVALID_ID;
    }

    if ( (ModeID = IGraphics->GetVPModeID(&My_Window->WScreen->ViewPort) ) == INVALID_ID)
    {
        return INVALID_ID;
    }

	mouse_hidden = FALSE;
	IIntuition->ClearPointer(My_Window);

	vo_dwidth = My_Window->Width - (My_Window->BorderLeft + My_Window->BorderRight);
	vo_dheight = My_Window->Height - (My_Window->BorderBottom + My_Window->BorderTop);
	vo_fs = 0;

    IIntuition->ScreenToFront(My_Window->WScreen);

    return ModeID;
}

static ULONG Open_FullScreen(void)
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
		
	ratio = (float)d_width/(float)d_height;

	if (d_width<MIN_WIDTH || d_height<MIN_HEIGHT)
	{
		d_width = MIN_WIDTH * ratio;
		d_height = MIN_HEIGHT * ratio;
	}
	//else
	//	d_height = d_width / ratio;

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

	image_width = width;
	image_height = height;
    image_format = format;

    vo_fs = flags & VOFLAG_FULLSCREEN;

	// backup info
	//image_width  = (width  - (width  % 8));
	//image_height = (height - (height % 8));

	window_width  = d_width; //(d_width  - (d_width  % 8));
	window_height = d_height; //(d_height - (d_height % 8));

    //printf("\nconfig: image_width:%d, image_height:%d\nwidth: %d, height: %d\nd_width: %d, d_height:%d",image_width, image_height, width, height, d_width, d_height);
    //printf("\nconfig: old_d_width:%d, old_d_height:%d\nkeep_width: %d, keep_height: %d",old_d_width, old_d_height, keep_width, keep_height);
    //printf("\nconfig: window_width:%d, window_height:%d\n",window_width, window_height);

	EmptyPointer = IExec->AllocVec(12, MEMF_PUBLIC | MEMF_CLEAR);

	if ( vo_fs )
	{
		ModeID = Open_FullScreen();
	}
	else
		ModeID = Open_PIPWindow();

    if (INVALID_ID == ModeID)
    {
       return 1;
    }

    rp = My_Window->RPort;
    UserMsg = My_Window->UserPort;

    vo_draw_alpha_func = draw_alpha_yv12;
    out_format = IMGFMT_YV12;

    if (PrepareVideo(format, out_format) < 0)
    {
        return 1;
    }

	if (hasGUI == TRUE)
	{
		TimerInit();
		TimerReset(RESET_TIME);
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
    int numbuffers=0;

/*
    w -= (w%8);
*/
    IP96->p96PIP_GetTags(My_Window, P96PIP_SourceBitMap   , (ULONG)&bm   ,
    								P96PIP_NumberOfBuffers, &numbuffers  ,
    								TAG_END, TAG_DONE );

    if (! (lock_h = IP96->p96LockBitMap(bm, (uint8 *)&ri, sizeof(struct RenderInfo))))
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

        srcdata = (ULONG *) ((uint32)ri.Memory + ( ( (y * image_width) + x) * 2));

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

	if (numbuffers>1)
	{
		uint32 WorkBuf, DispBuf, NextWorkBuf;

		IP96->p96PIP_GetTags(My_Window,	P96PIP_WorkBuffer, &WorkBuf,
								 		P96PIP_DisplayedBuffer, &DispBuf,
								 		TAG_END, TAG_DONE );

		NextWorkBuf = (WorkBuf+1) % numbuffers;

		if (NextWorkBuf == DispBuf) IGraphics->WaitTOF();

		IP96->p96PIP_SetTags(My_Window,	P96PIP_VisibleBuffer, WorkBuf,
								 		P96PIP_WorkBuffer,    NextWorkBuf,
								 		TAG_END, TAG_DONE );
	}

    IP96->p96UnlockBitMap(bm, lock_h);

    return 0;
}
/******************************** DRAW_OSD ******************************************/

static void draw_osd(void)
{
    vo_draw_text(image_width, image_height, vo_draw_alpha_func);
}
/******************************** FLIP_PAGE ******************************************/
static void flip_page(void)
{
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
	int i = 0;

	TimerExit();

	if (window_title)
	{
		IExec->FreeVec(window_title);
		window_title = NULL;
	}
	
    if (AppWin)
   	{
		IWorkbench->RemoveAppWindow(AppWin);
		AppWin = NULL;
   	}

	if (EmptyPointer)
	{
		if (My_Window) IIntuition->ClearPointer(My_Window);
		IExec->FreeVec(EmptyPointer);
		EmptyPointer = NULL;
	}
	
	if (My_Window)
    {
   	    IIntuition->ClearMenuStrip(My_Window);

		if (WinLayout)
		{
			IIntuition->DisposeObject(WinLayout);
			WinLayout = NULL;
		}

		for (i=0;i<7;i++)
		{
			if (NULL != image[i])
			{
				IIntuition->DisposeObject((Object *) image[i]);
				image[i] = NULL;
			}
		}

   	    IP96->p96PIP_Close(My_Window);
		if (IconifyGadget) 
		{
			IIntuition->DisposeObject(IconifyGadget);
			IconifyGadget = NULL;
		}

		if (IconifyImage) 
		{
			IIntuition->DisposeObject(IconifyImage);
			IconifyImage = NULL;
		}
       	My_Window = NULL;

	}
    if (My_Screen)
   	{
		IIntuition->CloseScreen(My_Screen);
		My_Screen = NULL;
   	}
}
/******************************** UNINIT    ******************************************/
static void uninit(void)
{
	if (player_idle_mode==0)
	{
	    FreeGfx();

	    p96_CloseOSLibs();
	}
	else
	{
    	keep_width = 0;
	    keep_height = 0;
    	old_d_width = 0;
	    old_d_height = 0;

		isPaused = TRUE;
		ChangePauseButton();
        put_command0(MP_CMD_PAUSE);
		isPaused = FALSE;
		Stopped = TRUE;
	}
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
			vo_fs = !vo_fs;

			FreeGfx(); // Free/Close all gfx stuff (screen windows, buffer...);
			if ( config(image_width, image_height, window_width, window_height, vo_fs, NULL, image_format) < 0) return VO_FALSE;

            return VO_TRUE;
		break;
        case VOCTRL_ONTOP:
        	vo_ontop = !vo_ontop;
        	is_ontop = !is_ontop;
        	
			FreeGfx(); // Free/Close all gfx stuff (screen windows, buffer...);
			if ( config(image_width, image_height, window_width, window_height, FALSE, NULL, image_format) < 0) return VO_FALSE;
            return VO_TRUE;
		break;
		case VOCTRL_UPDATE_SCREENINFO:
			if (vo_fs)
			{
				vo_screenwidth = My_Screen->Width;
				vo_screenheight = My_Screen->Height;
			}
			else
			{
				struct Screen *scr = IIntuition->LockPubScreen (NULL);
				if(scr)
				{
					vo_screenwidth = scr->Width;
					vo_screenheight = scr->Height;
					IIntuition->UnlockPubScreen(NULL, scr);
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
    p96_CheckEvents(My_Window, &window_height, &window_width, &win_left, &win_top);
/* ARexx */
    if(IExec->SetSignal(0L,rxHandler->sigmask) & rxHandler->sigmask)
    {
        IIntuition->IDoMethod(rxHandler->rxObject,AM_HANDLEEVENT);
    }
/* ARexx */
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

