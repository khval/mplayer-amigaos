/*
 *  vo_p96.c
 *  VO module for MPlayer AmigaOS4
 *  using p96
 *  Written by Varthall and Jörg Strohmayer
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
#include "../libmpdemux/demuxer.h"

#include "vo_p96.h"

#include <assert.h>
#undef CONFIG_GUI

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

	secs=0, mics=0, secs2=0, mics2=0;
	p_mics1=0, p_mics2=0, p_secs1=0, p_secs2=0;

	MPLAppIcon = NULL;

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
			p96_TimerReset(RESET_TIME);
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
	                                        	file = p96_LoadFile(NULL);

    	                                    	if (file)
    	                                    	{
    	                                    		#ifdef CONFIG_GUI
													guiGetEvent(guiCEvent, (void *)guiSetPlay);
													#endif
													p96_NotifyServer(file);
													p96_PlayFile((char*)file);
    	                                    	}
    	                                    }
                                        break;
                                        case 1:  /* Open DVD */
                                        {
											struct Process *DvdMessage;
											if (choosing == 0)
											{
												DvdMessage = (struct Process *) IDOS->CreateNewProcTags (
		   													NP_Entry, 		(ULONG) p96_OpenDVD,
						   									NP_Name,	   	"Load DVD",
															NP_StackSize,   262144,
															NP_Child,		TRUE,
															NP_Priority,	0,
								   							NP_ExitData, 	IDOS,
						  									TAG_DONE);
												if (!DvdMessage)
													p96_PrintMsg(errmsg,REQTYPE_INFO,REQIMAGE_ERROR);
											}
										}
										break;
										case 2:
										{
											struct Process *VcdMessage;
											if (choosing == 0)
											{
												VcdMessage = (struct Process *) IDOS->CreateNewProcTags (
		   													NP_Entry, 		(ULONG) p96_OpenVCD,
						   									NP_Name,	   	"Load VCD",
															NP_StackSize,   262144,
															NP_Child,		TRUE,
															NP_Priority,	0,
								   							NP_ExitData, 	IDOS,
						  									TAG_DONE);
												if (!VcdMessage)
													p96_PrintMsg(errmsg,REQTYPE_INFO,REQIMAGE_ERROR);
											}
										}
										break;
										case 3:
										{
											struct Process *NetworkMessage;
											if (choosing == 0)
											{
												NetworkMessage = (struct Process *) IDOS->CreateNewProcTags (
		   													NP_Entry, 		(ULONG) p96_OpenNetwork,
						   									NP_Name,	   	"Load Network",
															NP_StackSize,   262144,
															NP_Child,		TRUE,
															NP_Priority,	0,
								   							NP_ExitData, 	IDOS,
						  									TAG_DONE);
												if (!NetworkMessage)
													p96_PrintMsg(errmsg,REQTYPE_INFO,REQIMAGE_ERROR);
											}
										}
										break;
										case 5:
											p96_ShowAbout();
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

											p96_ChangePauseButton();
                                            put_command0(MP_CMD_PAUSE);
                                        break;

                                        case 1:  /* Quit */
                                            put_command0(MP_CMD_PAUSE);
											isPaused = TRUE;
											p96_ChangePauseButton();
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
	                                        p96_ChangeWindowSize(x1);
                                        break;
                                        case 6:
	                                        p96_ChangeWindowSize(x2);
                                        break;
                                        case 7:
	                                        p96_ChangeWindowSize(xF);
                                        break;
                                        case 8:
	                                        p96_ChangeWindowSize(xH);
                                        break;
                                        case 10:
	                                        switch (SUBNUM(menucode))
    	                                    {
    	                                    	case 0:
		    	                                    AspectRatio = AR_ORIGINAL;
	                        	    	            p96_ChangeWindowSize(AR_ORIGINAL);
                                	    	    break;
                                	    	    case 2:
		    	                                    AspectRatio = AR_16_10;
	                        	    	            p96_ChangeWindowSize(AR_16_10);
    	    	                                case 3:
		    	                                    AspectRatio = AR_16_9;
	    	    	                                p96_ChangeWindowSize(AR_16_9);
            	    	                        break;
                                    	    	case 4:
		    	                                    AspectRatio = AR_185_1;
		                                    	    p96_ChangeWindowSize(AR_185_1);
		                                        break;
    	    	                                case 5:
		    	                                    AspectRatio = AR_221_1;
	    	    	                                p96_ChangeWindowSize(AR_221_1);
            	    	                        break;
    	    	                                case 6:
		    	                                    AspectRatio = AR_235_1;
	    	    	                                p96_ChangeWindowSize(AR_235_1);
            	    	                        break;
    	    	                                case 7:
		    	                                    AspectRatio = AR_239_1;
	    	    	                                p96_ChangeWindowSize(AR_239_1);
            	    	                        break;
    	    	                                case 8:
		    	                                    AspectRatio = AR_5_3;
	    	    	                                p96_ChangeWindowSize(AR_5_3);
            	    	                        break;
    	    	                                case 9:
		    	                                    AspectRatio = AR_4_3;
	    	    	                                p96_ChangeWindowSize(AR_4_3);
            	    	                        break;
    	    	                                case 10:
		    	                                    AspectRatio = AR_5_4;
	    	    	                                p96_ChangeWindowSize(AR_5_4);
            	    	                        break;
    	    	                                case 11:
		    	                                    AspectRatio = AR_1_1;
	    	    	                                p96_ChangeWindowSize(AR_1_1);
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
		                                    p96_SetVolume(VOL_UP);
		                                break;
		                                case 2:
		                                    p96_SetVolume(VOL_DOWN);
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

							p96_ChangePauseButton();
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

								p96_ChangePauseButton();
								if (!Stopped)
									put_command0(MP_CMD_PAUSE);
								else
								{
									if (current_filename)
										p96_PlayFile(current_filename);
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
								p96_ChangePauseButton();
								isPaused = FALSE;
							}
							break;
							case GID_Eject:
								if (choosing == 0)
								{
                                    file = p96_LoadFile(NULL);

    	                            if (file)
    	                            {
    	                            	#ifdef CONFIG_GUI
										guiGetEvent(guiCEvent, (void *)guiSetPlay);
										#endif
										p96_NotifyServer(file);
										p96_PlayFile((char*)file);
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
		    	        	p96_TimerReset(RESET_TIME);
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
			            	p96_TimerReset(RESET_TIME);
			            }
					}
				}
	            break;
            }
        }
    }

    p96_appw_events();

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
 	// to be implemented - find non-PIP equivalent of p96PIP_GetTags()
	return;
}

/******************************** PREINIT ******************************************/
static int preinit(const char *arg)
{
    mp_msg(MSGT_VO, MSGL_INFO, "VO: [p96]\n");
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
		IIntuition->CloseWindow(My_Window);
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

	My_Screen = IIntuition->OpenScreenTags (
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

    My_Window = IIntuition->OpenWindowTags(
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
static ULONG Open_Window(void)
{
    // Window
    ULONG ModeID = INVALID_ID;
    APTR vi;
	DIcon = NULL;

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
								My_Window = IIntuition->OpenWindowTags(NULL,
									WA_Width, image_width,
  									WA_Height, image_height,
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
								My_Window = IIntuition->OpenWindowTags(NULL,
									WA_Width, image_width,
  									WA_Height, image_height,
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
								My_Window = IIntuition->OpenWindowTags(NULL,
									WA_Width, image_width,
  									WA_Height, image_height,
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
// = TRUE for GUI
                                hasGUI = FALSE;
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

	window_width  = d_width;
	window_height = d_height;

    //printf("\nconfig: image_width:%d, image_height:%d\nwidth: %d, height: %d\nd_width: %d, d_height:%d",image_width, image_height, width, height, d_width, d_height);
    //printf("\nconfig: old_d_width:%d, old_d_height:%d\nkeep_width: %d, keep_height: %d",old_d_width, old_d_height, keep_width, keep_height);
    //printf("\nconfig: window_width:%d, window_height:%d\n",window_width, window_height);

	EmptyPointer = IExec->AllocVec(12, MEMF_PUBLIC | MEMF_CLEAR);

	if ( vo_fs )
	{
		ModeID = Open_FullScreen();
	}
	else
		ModeID = Open_Window();

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
		p96_TimerInit();
		p96_TimerReset(RESET_TIME);
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
 	// to be implemented - find non-PIP equivalent of p96PIP_GetTags()

    return -1;

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

	p96_TimerExit();

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

   	    IIntuition->CloseWindow(My_Window);
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
		p96_ChangePauseButton();
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

void p96_ChangePauseButton(void)
{
	Object *temp = NULL;
	struct Screen *tempScreen = IIntuition->LockPubScreen("Workbench");

	IIntuition->SetGadgetAttrs((struct Gadget *)OBJ[GID_MAIN], My_Window, NULL,
		LAYOUT_ModifyChild, OBJ[GID_Pause],
			CHILD_ReplaceObject, 				temp = ButtonObject,
					GA_ID, 							GID_Pause,
					GA_ReadOnly, 					FALSE,
					GA_RelVerify, 					TRUE,
					BUTTON_Transparent,				TRUE,
					BUTTON_BevelStyle, 				BVS_NONE,
					BUTTON_RenderImage, 		image[1] =  BitMapObject, 		//Pause
							BITMAP_Screen, 				tempScreen,
							BITMAP_Masking, 			TRUE,
							BITMAP_Transparent, 		TRUE,
							BITMAP_SourceFile, 			isPaused == TRUE ? "PROGDIR:images/td_play.png"	   : "PROGDIR:images/td_pause.png",
							BITMAP_SelectSourceFile, 	isPaused == TRUE ? "PROGDIR:images/td_play_s.png"  : "PROGDIR:images/td_pause_s.png",
							BITMAP_DisabledSourceFile, 	isPaused == TRUE ? "PROGDIR:images/td_play_g.png"  : "PROGDIR:images/td_pause_g.png",
						BitMapEnd,
					ButtonEnd,
		TAG_DONE);

	OBJ[GID_Pause] = temp;
	IIntuition->UnlockPubScreen(NULL, tempScreen);
	ILayout->RethinkLayout((struct Gadget *)OBJ[GID_MAIN], My_Window, NULL, TRUE);
}

struct Gadget *CreateWinLayout(struct Screen *screen, struct Window *window)
{
	Object *TempLayout;
	TempLayout = VLayoutObject,
					GA_Left, window->BorderLeft,
					GA_Top, window->BorderTop,
					GA_RelWidth, -(window->BorderLeft + window->BorderRight),
					GA_RelHeight, -(window->BorderTop + window->BorderBottom),
					ICA_TARGET, 		ICTARGET_IDCMP,
					LAYOUT_DeferLayout, FALSE,
					LAYOUT_RelVerify, 	TRUE,
                    LAYOUT_AddChild, VLayoutObject,
						LAYOUT_AddChild, Space_Object = SpaceObject,
							GA_ReadOnly, TRUE,
							//SPACE_MinWidth, image_width,
							//SPACE_MinHeight, image_height,
							SPACE_Transparent, TRUE,
						SpaceEnd,
					LayoutEnd,
                    CHILD_WeightedWidth, 100,
                    CHILD_WeightedHeight, 100,
					LAYOUT_AddChild, ToolBarObject = (struct Gadget*)HLayoutObject,
						LAYOUT_VertAlignment, LALIGN_CENTER,
						StartMember, OBJ[GID_Back] = ButtonObject,
							GA_ID, GID_Back,
							GA_ReadOnly, FALSE,
							GA_RelVerify, TRUE,
							BUTTON_Transparent,TRUE,
							BUTTON_BevelStyle, BVS_NONE,
							BUTTON_RenderImage,image[0] = BitMapObject,
								BITMAP_SourceFile,"PROGDIR:images/td_rewind.png",
								BITMAP_SelectSourceFile,"PROGDIR:images/td_rewind_s.png",
								BITMAP_DisabledSourceFile,"PROGDIR:images/td_rewind_g.png",
								BITMAP_Screen,screen,
								BITMAP_Masking,TRUE,
							BitMapEnd,
						EndMember,
						CHILD_WeightedWidth, 0,
						CHILD_WeightedHeight, 0,
						StartMember, OBJ[GID_Pause] = ButtonObject,
							GA_ID, GID_Pause,
							GA_ReadOnly, FALSE,
							GA_RelVerify, TRUE,
							BUTTON_Transparent,TRUE,
							BUTTON_BevelStyle, BVS_NONE,
							BUTTON_RenderImage,image[1] = BitMapObject,
								BITMAP_SourceFile,"PROGDIR:images/td_pause.png",
								BITMAP_SelectSourceFile,"PROGDIR:images/td_pause_s.png",
								BITMAP_DisabledSourceFile,"PROGDIR:images/td_pause_g.png",
								BITMAP_Screen,screen,
								BITMAP_Masking,TRUE,
							BitMapEnd,
						EndMember,
						CHILD_WeightedWidth, 0,
						CHILD_WeightedHeight, 0,
						StartMember, OBJ[GID_Forward] = ButtonObject,
							GA_ID, GID_Forward,
							GA_ReadOnly, FALSE,
							GA_RelVerify, TRUE,
							BUTTON_Transparent,TRUE,
							BUTTON_BevelStyle, BVS_NONE,
							BUTTON_RenderImage,image[2] = BitMapObject,
								BITMAP_SourceFile,"PROGDIR:images/td_ff.png",
								BITMAP_SelectSourceFile,"PROGDIR:images/td_ff_s.png",
								BITMAP_DisabledSourceFile,"PROGDIR:images/td_ff_g.png",
								BITMAP_Screen,screen,
								BITMAP_Masking,TRUE,
							BitMapEnd,
						EndMember,
						CHILD_WeightedWidth, 0,
						CHILD_WeightedHeight, 0,
						StartMember, OBJ[GID_Stop] = ButtonObject,
							GA_ID, GID_Stop,
							GA_ReadOnly, FALSE,
							GA_RelVerify, TRUE,
							BUTTON_Transparent,TRUE,
							BUTTON_BevelStyle, BVS_NONE,
							BUTTON_RenderImage,image[3] = BitMapObject,
								BITMAP_SourceFile,"PROGDIR:images/td_stop.png",
								BITMAP_SelectSourceFile,"PROGDIR:images/td_stop_s.png",
								BITMAP_DisabledSourceFile,"PROGDIR:images/td_stop_g.png",
								BITMAP_Screen,screen,
								BITMAP_Masking,TRUE,
							BitMapEnd,
						EndMember,
						CHILD_WeightedWidth, 0,
						CHILD_WeightedHeight, 0,
						StartMember, OBJ[GID_Eject] = ButtonObject,
							GA_ID, GID_Eject,
							GA_ReadOnly, FALSE,
							GA_RelVerify, TRUE,
							BUTTON_Transparent,TRUE,
							BUTTON_BevelStyle, BVS_NONE,
							BUTTON_RenderImage,image[4] = BitMapObject,
								BITMAP_SourceFile,			"PROGDIR:images/td_eject.png",
								BITMAP_SelectSourceFile,	"PROGDIR:images/td_eject_s.png",
								BITMAP_DisabledSourceFile,	"PROGDIR:images/td_eject_g.png",
								BITMAP_Screen,screen,
								BITMAP_Masking,TRUE,
							BitMapEnd,
						EndMember,
						CHILD_WeightedWidth, 0,
						CHILD_WeightedHeight, 0,
						StartMember, OBJ[GID_Time] = ScrollerObject,
							GA_ID, 					GID_Time,
							GA_RelVerify, 			TRUE,
							GA_FollowMouse,			TRUE,
							GA_Immediate, 			TRUE,
							ICA_TARGET, 			ICTARGET_IDCMP,
							SCROLLER_Total, 		100,
							SCROLLER_Arrows, 		FALSE,
							SCROLLER_Orientation, 	SORIENT_HORIZ,
							PGA_ArrowDelta,			10000,
							PGA_Freedom, 			FREEHORIZ,
						End,
						CHILD_MinHeight, screen->Font->ta_YSize + 6,
						StartMember, OBJ[GID_FullScreen] = ButtonObject,
							GA_ID, 				GID_FullScreen,
							GA_ReadOnly, 		FALSE,
							GA_RelVerify, 		TRUE,
							BUTTON_Transparent,	TRUE,
							BUTTON_BevelStyle, 	BVS_NONE,
							BUTTON_RenderImage,image[5] = BitMapObject,
								BITMAP_SourceFile,			"PROGDIR:images/td_fullscreen.png",
								BITMAP_SelectSourceFile,	"PROGDIR:images/td_fullscreen_s.png",
								BITMAP_DisabledSourceFile,	"PROGDIR:images/td_fullscreen_g.png",
								BITMAP_Screen,				screen,
								BITMAP_Masking,				TRUE,
							BitMapEnd,
						EndMember,
						CHILD_WeightedWidth, 	0,
						CHILD_WeightedHeight, 	0,
						StartMember, OBJ[GID_Volume] = ScrollerObject,
							GA_ID, 					GID_Volume,
							GA_RelVerify, 			TRUE,
							GA_FollowMouse,			TRUE,
							GA_Immediate, 			TRUE,
							ICA_TARGET, 			ICTARGET_IDCMP,
							SCROLLER_Total, 		100,
							SCROLLER_Top, 			100,
							SCROLLER_Arrows, 		FALSE,
							SCROLLER_Orientation, 	SORIENT_HORIZ,
							PGA_ArrowDelta,			50,
							PGA_Freedom, 			FREEHORIZ,
						End,
						CHILD_MinHeight, screen->Font->ta_YSize + 6,
						CHILD_MinWidth, 50,
						CHILD_WeightedWidth, 0,
					LayoutEnd,
                    CHILD_WeightedHeight, 0,
				LayoutEnd;

	return (struct Gadget *)TempLayout;
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

static int32 p96_PlayFile(const char *FileName)
{
	mp_cmd_t MPCmd;
	mp_cmd_t *c;
	ULONG iValue = 0;
	int abs = 2;

	/* Reset some variables.. */
	old_d_width  = 0;
	old_d_height = 0;
   	keep_width = 0;
    keep_height = 0;
    image_width = 0;
    image_height = 0;

	FirstTime = TRUE;

	/* Stop the current file */
	IIntuition->GetAttr(SCROLLER_Total, OBJ[GID_Time], &iValue);
	MPCmd.pausing = FALSE;
	MPCmd.id    = MP_CMD_SEEK;
	MPCmd.name  = strdup("seek");
	MPCmd.nargs = 2;
	MPCmd.args[0].type = MP_CMD_ARG_FLOAT;
	MPCmd.args[0].v.f=iValue/1000.0f;
	MPCmd.args[1].type = MP_CMD_ARG_INT;
	MPCmd.args[1].v.i=abs;
	MPCmd.args[2].type = -1;
	MPCmd.args[2].v.i=0;
	if (( c = mp_cmd_clone(&MPCmd)))
		if ((mp_input_queue_cmd(c)==0))
		{
			mp_cmd_free(c);
		}
	p96_ChangePauseButton();

	/* Add the file to the queue */
	MPCmd.id			= MP_CMD_LOADFILE;
	MPCmd.name			= NULL;
	MPCmd.nargs			= 2;
	MPCmd.pausing		= FALSE;
	MPCmd.args[0].type	= MP_CMD_ARG_STRING;
	MPCmd.args[0].v.s	= (char*)FileName;
	MPCmd.args[1].type	= MP_CMD_ARG_INT;
	MPCmd.args[1].v.i	= TRUE;
	MPCmd.args[2]		= mp_cmds[MP_CMD_LOADFILE].args[2];
	if (( c = mp_cmd_clone(&MPCmd)))
		if ((mp_input_queue_cmd(c)==0))
		{
			mp_cmd_free(c);
		}

	/* Play it! */
	MPCmd.id			= Stopped==FALSE ? MP_CMD_PLAY_TREE_STEP : MP_CMD_PLAY_TREE_UP_STEP;
	MPCmd.args[0].type	= MP_CMD_ARG_INT;
	MPCmd.args[0].v.i	= 1;
	MPCmd.args[1].type	= MP_CMD_ARG_INT;
	MPCmd.args[1].v.i	= TRUE;
	MPCmd.args[2]		= mp_cmds[MP_CMD_PLAY_TREE_STEP].args[2];

	if ((c = mp_cmd_clone(&MPCmd)))
		if ((mp_input_queue_cmd(c)==0))
		{
			mp_cmd_free(c);
		}

	isPaused = FALSE;
	p96_ChangePauseButton();

	return RETURN_OK;
}

static void p96_NotifyServer(STRPTR FileName)
{
	/* Send a message to Notification System (AKA Ringhio.. ;) */
	if (hasNotificationSystem == TRUE)
	{
		uint32 result  = 0;
		int32  success = 0;
		STRPTR iconname = NULL;
		static char text[512];

		sprintf(text,"Playing %s..",FileName);
		iconname = IExec->AllocVec(512,MEMF_SHARED | MEMF_CLEAR);
		if (iconname)
		{
			success = IDOS->NameFromLock(IDOS->GetCurrentDir(),iconname,512);
			if (success)
			{
				strcat(iconname,"/images/mplayer.png");
			}
		}
		result = IApplication->Notify(AppID,
										APPNOTIFY_Title,			"MPlayer",
										APPNOTIFY_Update,			TRUE,
										APPNOTIFY_PubScreenName,	"FRONT",
										APPNOTIFY_ImageFile,		(STRPTR)iconname,
										APPNOTIFY_Text,				text,
									TAG_DONE);

		if (iconname) IExec->FreeVec(iconname);
	}
}

void p96_RemoveAppPort(void)
{
    if (AppPort)
    {
		void *msg;
		while ((msg = IExec->GetMsg(AppPort)))
		    IExec->ReplyMsg(msg);
		IExec->FreeSysObject(ASOT_PORT, AppPort);
		AppPort = NULL;
    }
}

static void p96_appw_events (void)
{
    if (AppWin)
    {
		struct AppMessage *msg;
		struct WBArg *arg;
	    char cmd[512] = {0};
		
		if (AppPort)
		{
			while ((msg = (void*) IExec->GetMsg(AppPort)))
			{
			    arg = msg->am_ArgList;

		    	IDOS->NameFromLock(arg[0].wa_Lock, &cmd[0], 512);

				if ((strcmp(arg[0].wa_Name,"")!=0))
				{
				    IDOS->AddPart(cmd,arg[0].wa_Name,512);
					p96_NotifyServer(cmd);
					p96_PlayFile(cmd);
				}
				else
				{
					char *file;
					file = p96_LoadFile(cmd);
					if (file)
					{
						p96_NotifyServer(file);
						p96_PlayFile(file);
					}
				}
		    	IExec->ReplyMsg ((void*) msg);
			}
		}
    }
}

/* STUFF FUNCTIONS FOR MENU */
static void p96_CalcAspectRatio(float a_ratio, int _oldwidth, int _oldheight, int *_newwidth, int *_newheight)
{
	int tempLen = 0;

	tempLen = _oldwidth / a_ratio;
	*_newheight = tempLen;
	*_newwidth = _oldwidth;
}

static void p96_ChangeWindowSize(int mode)
{
	int new_width = 0, new_height = 0;
	int new_left  = 0, new_top    = 0;
	int new_bw    = 0, new_bh     = 0;
	struct Screen *tempScreen = NULL;

    if ( ( tempScreen = IIntuition->LockPubScreen ( NULL ) ) )
    {
	    new_bw = tempScreen->WBorLeft + tempScreen->WBorRight;
		new_bh = tempScreen->WBorTop + tempScreen->Font->ta_YSize + 1 + tempScreen->WBorBottom;

		switch (mode)
		{
			case x1:
			case AR_ORIGINAL:
				new_width = keep_width;
				new_height = keep_height;
			break;
			case x2:
				new_width = keep_width * 2;
				new_height = keep_height * 2;
			break;
			case xF:
				new_width = My_Screen->Width - new_bw;
				new_height = My_Screen->Height - tempScreen->BarHeight - new_bh;
			break;
			case xH:
				new_width = keep_width / 2;
				new_height = keep_height / 2;
			break;
			case AR_4_3:
				p96_CalcAspectRatio(1.333333f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_5_3:
				p96_CalcAspectRatio(1.666667f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_5_4:
				p96_CalcAspectRatio(1.25f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_16_10:
				p96_CalcAspectRatio(1.6f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_16_9:
				p96_CalcAspectRatio(1.777777f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_235_1:
				p96_CalcAspectRatio(2.35f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_239_1:
				p96_CalcAspectRatio(2.39f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_221_1:
				p96_CalcAspectRatio(2.21f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_185_1:
				p96_CalcAspectRatio(1.85f, keep_width, keep_height, &new_width, &new_height);
			break;
		}
		if (mode!=xF)
		{
			new_left = (tempScreen->Width - (new_width + new_bw)) / 2;
			new_top  = (tempScreen->Height - (new_height + new_bh)) / 2;
		}
		else
		{
			new_left = 0;
			new_top  = tempScreen->BarHeight;
		}
		
		if (new_top<0)
		{
			new_top 	= tempScreen->BarHeight;
			new_height 	= new_height - tempScreen->BarHeight;
		}
		if (new_left<0)
		{
			new_left 	= tempScreen->WBorRight;
			new_width 	= new_width - tempScreen->WBorRight;
		}

	    IIntuition->SetWindowAttrs(My_Window,
                               		WA_Left,		new_left,
                               		WA_Top,			new_top,
                               		WA_InnerWidth,	new_width,
                               		WA_InnerHeight,	new_height,
                               		TAG_DONE);
	}

}

static void p96_SetVolume(int direction)
{
	switch (direction)
	{
		case VOL_UP:
			mplayer_put_key(KEY_VOLUME_UP);
		break;
		case VOL_DOWN:
			mplayer_put_key(KEY_VOLUME_DOWN);
		break;
	}
}

static UBYTE *p96_LoadFile(const char *StartingDir)
{
	struct FileRequester * AmigaOS_FileRequester = NULL;
	BPTR FavoritePath_File;
	char FavoritePath_Value[1024];
	BOOL FavoritePath_Ok = FALSE;
	UBYTE *filename = NULL;
	int len = 0;

	if (StartingDir==NULL)
	{
		FavoritePath_File = IDOS->Open("PROGDIR:FavoritePath", MODE_OLDFILE);
		if (FavoritePath_File)
		{
			LONG size = IDOS->Read(FavoritePath_File, FavoritePath_Value, 1024);
			if (size > 0)
			{
				if ( strchr(FavoritePath_Value, '\n') ) // There is an /n -> valid file
				{
					FavoritePath_Ok = TRUE;
					*(strchr(FavoritePath_Value, '\n')) = '\0';
				}
			}
			IDOS->Close(FavoritePath_File);
		}
	}
	else
	{
		FavoritePath_Ok = TRUE;
		strcpy(FavoritePath_Value, StartingDir);
	}

	AmigaOS_FileRequester = IAsl->AllocAslRequest(ASL_FileRequest, NULL);
	if (!AmigaOS_FileRequester)
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL,"Cannot open the FileRequester!\n");
		return NULL;
	}

	choosing = 1;
	if ( ( IAsl->AslRequestTags( AmigaOS_FileRequester,
					ASLFR_TitleText,        "Choose a video",
					ASLFR_DoMultiSelect,    FALSE, //maybe in a future we can implement a playlist..
					ASLFR_RejectIcons,      TRUE,
					ASLFR_DoPatterns,		TRUE,
					ASLFR_StayOnTop,		TRUE,
					ASLFR_InitialPattern,	(ULONG)EXTPATTERN,
		    		ASLFR_InitialDrawer,    (FavoritePath_Ok) ? FavoritePath_Value : "",
					TAG_DONE) ) == FALSE )
	{
		IAsl->FreeAslRequest(AmigaOS_FileRequester);
		AmigaOS_FileRequester = NULL;
		choosing = 0;
		return NULL;
	}
	choosing = 0;

	if (AmigaOS_FileRequester->fr_NumArgs>0);
	{
		len = strlen(AmigaOS_FileRequester->fr_Drawer) + strlen(AmigaOS_FileRequester->fr_File) + 1;
		if ((filename = IExec->AllocMem(len,MEMF_SHARED|MEMF_CLEAR)))
		{
    	    strcpy(filename,AmigaOS_FileRequester->fr_Drawer);
        	IDOS->AddPart(filename,AmigaOS_FileRequester->fr_File,len + 1);
		}
	}

	IAsl->FreeAslRequest(AmigaOS_FileRequester);
	return filename;
}

static int32 p96_PrintMsgProc(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase)
{
        struct ExecIFace *iexec = (APTR)execbase->MainInterface;
        struct Process *me = (APTR)iexec->FindTask(NULL);
        struct DOSIFace *idos = (APTR)me->pr_ExitData;
        struct pmpMessage *pmp = (struct pmpMessage *)idos->GetEntryData();

        if(pmp)
        {
                p96_PrintMsg(pmp->about, pmp->type, pmp->image);
                iexec->FreeVec(pmp);
                return RETURN_OK;
        }
        else
        	return RETURN_FAIL;

}

static void p96_PrintMsg(CONST_STRPTR text,int REQ_TYPE, int REQ_IMAGE)
{
	Object *reqobj;

	if (REQ_TYPE  == 0) REQ_TYPE	= REQTYPE_INFO;
	if (REQ_IMAGE == 0) REQ_IMAGE	= REQIMAGE_DEFAULT;

	reqobj = (Object *)IIntuition->NewObject( IRequester->REQUESTER_GetClass(), NULL,
            									REQ_Type,       REQ_TYPE,
												REQ_TitleText,  "MPlayer for AmigaOS4",
									            REQ_BodyText,   text,
									            REQ_Image,      REQ_IMAGE,
												REQ_TimeOutSecs, 10,
									            REQ_GadgetText, "Ok",
									            TAG_END
        );

	if ( reqobj )
	{
		IIntuition->IDoMethod( reqobj, RM_OPENREQ, NULL, My_Window, NULL, TAG_END );
		IIntuition->DisposeObject( reqobj );
	}
}

static void p96_ShowAbout(void)
{
	struct Process *TaskMessage;
	struct pmpMessage *pmp = IExec->AllocVec(sizeof(struct pmpMessage), MEMF_SHARED);

	if(!pmp)
        return;

	pmp->about = "MPlayer - The Video Player\n"
			  	 "\nPianeta Amiga Version"
    			 "\n\nBuilt against MPlayer version: "	VERSION
				 "\n\nCopyright (C) MPlayer Team - http://www.mplayerhq.hu"
				 "\nAmigaOS4 Version by Andrea Palmate' - http://www.amigasoft.net";

	pmp->type = REQTYPE_INFO;
	pmp->image = REQIMAGE_INFO;

	TaskMessage = (struct Process *) IDOS->CreateNewProcTags (
		   										NP_Entry, 		(ULONG) p96_PrintMsgProc,
									   			NP_Name,	   	"About MPlayer",
												NP_StackSize,   262144,
												NP_Child,		TRUE,
												NP_Priority,	0,
					   							NP_EntryData, 	pmp,
					   							NP_ExitData, 	IDOS,
									  			TAG_DONE);
	if (!TaskMessage)
	{
		p96_PrintMsg(pmp->about,REQTYPE_INFO,REQIMAGE_INFO);
		IExec->FreeVec(pmp);
	}
}

static void p96_OpenNetwork(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED)
{
	Object *netwobj;
	UBYTE buffer[513]="http://";
	ULONG result;

	netwobj = (Object *) IIntuition->NewObject(IRequester->REQUESTER_GetClass(),NULL,
									REQ_Type,       REQTYPE_STRING,
									REQ_TitleText,  "Load Network",
									REQ_BodyText,   "Enter the URL to Open",
									REQ_GadgetText, "L_oad|_Cancel",
                  					REQS_Invisible,   FALSE,
                  					REQS_Buffer,      buffer,
									REQS_ShowDefault, TRUE,
									REQS_MaxChars,    512,
									TAG_DONE);
	if ( netwobj )
	{
		choosing = 1;
		result = IIntuition->IDoMethod( netwobj, RM_OPENREQ, NULL, NULL, NULL, TAG_END );
		if (result != 0 && (strcmp(buffer,"http://")!=0))
		{
       		#ifdef CONFIG_GUI
			guiGetEvent(guiCEvent, (void *)guiSetPlay);
			#endif
			p96_PlayFile(buffer);
		}
		IIntuition->DisposeObject( netwobj );
		choosing = 0;
	}
}

static void p96_OpenDVD(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED)
{
	Object *dvdobj;
	UBYTE buffer[256]="dvd://1";
	ULONG result;

	dvdobj = (Object *) IIntuition->NewObject(IRequester->REQUESTER_GetClass(),NULL,
									REQ_Type,       REQTYPE_STRING,
									REQ_TitleText,  "Load DVD",
									REQ_BodyText,   "Enter Chapter to Open",
									REQ_GadgetText, "O_pen|_Cancel",
                  					REQS_Invisible,   FALSE,
                  					REQS_Buffer,      buffer,
									REQS_ShowDefault, TRUE,
									REQS_MaxChars,    255,
									TAG_DONE);
	if ( dvdobj )
	{
		choosing = 1;
		result = IIntuition->IDoMethod( dvdobj, RM_OPENREQ, NULL, NULL, NULL, TAG_END );
		if (result != 0 && (strncmp(buffer,"dvd://",6)==0))
		{
       		#ifdef CONFIG_GUI
			guiGetEvent(guiCEvent, (void *)guiSetPlay);
			#endif
			p96_PlayFile(buffer);
			printf("buffer:%s\n",buffer);
		}
		else
			if (strncmp(buffer,"dvd://",6)!=0)
				p96_PrintMsg("Enter a valid DVD protocol", REQTYPE_INFO, REQIMAGE_ERROR);
		IIntuition->DisposeObject( dvdobj );
		choosing = 0;
	}
}

static void p96_OpenVCD(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED)
{
	Object *vcdobj;
	UBYTE buffer[256]="vcd://1";
	ULONG result;

	vcdobj = (Object *) IIntuition->NewObject(IRequester->REQUESTER_GetClass(),NULL,
									REQ_Type,       REQTYPE_STRING,
									REQ_TitleText,  "Load VCD",
									REQ_BodyText,   "Enter Chapter to Open",
									REQ_GadgetText, "O_pen|_Cancel",
                  					REQS_Invisible,   FALSE,
                  					REQS_Buffer,      buffer,
									REQS_ShowDefault, TRUE,
									REQS_MaxChars,    255,
									TAG_DONE);
	if ( vcdobj )
	{
		choosing = 1;
		result = IIntuition->IDoMethod( vcdobj, RM_OPENREQ, NULL, NULL, NULL, TAG_END );
		if (result != 0 && (strncmp(buffer,"vcd://",6)==0))
		{
       		#ifdef CONFIG_GUI
			guiGetEvent(guiCEvent, (void *)guiSetPlay);
			#endif
			p96_PlayFile(buffer);
		}
		else
			if (strncmp(buffer,"vcd://",6)!=0)
				p96_PrintMsg("Enter a valid VCD protocol", REQTYPE_INFO, REQIMAGE_ERROR);
		IIntuition->DisposeObject( vcdobj );
		choosing = 0;
	}
}

void p96_TimerReset(uint32 microDelay)
{
	if (microDelay>0)
	{
		_timerio->Request.io_Command = TR_ADDREQUEST;
		_timerio->Time.Seconds = 0;
		_timerio->Time.Microseconds = microDelay;
		IExec->SendIO((struct IORequest *)_timerio);
	}
}

BOOL p96_TimerInit(void)
{
	_port = (struct MsgPort *)IExec->AllocSysObject(ASOT_PORT, NULL);
	if (!_port) return FALSE;

	_timerSig = 1L << _port->mp_SigBit;

	_timerio = (struct TimeRequest *)IExec->AllocSysObjectTags(ASOT_IOREQUEST,
		ASOIOR_Size,		sizeof(struct TimeRequest),
		ASOIOR_ReplyPort,	_port,
	TAG_DONE);

	if (!_timerio) return FALSE;

	if (!IExec->OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *)
		_timerio, 0))
	{
		ITimer = (struct TimerIFace *)IExec->GetInterface((struct Library *)
			_timerio->Request.io_Device, "main", 1, NULL);
		if (ITimer) return TRUE;
	}

	return FALSE;
}

void p96_TimerExit(void)
{
	if (_timerio)
	{
		if (!IExec->CheckIO((struct IORequest *)_timerio))
		{
			IExec->AbortIO((struct IORequest *)_timerio);
			IExec->WaitIO((struct IORequest *)_timerio);
		}
	}

	if (_port)
	{
		IExec->FreeSysObject(ASOT_PORT, _port);
		_port = 0;
	}

	if (_timerio && _timerio->Request.io_Device)
	{
		IExec->CloseDevice((struct IORequest *)_timerio);
		IExec->FreeSysObject(ASOT_IOREQUEST, _timerio);
		_timerio = 0;
	}

	if (ITimer)
	{
		IExec->DropInterface((struct Interface *)ITimer);
		ITimer = 0;
	}
}
