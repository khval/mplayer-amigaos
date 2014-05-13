
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/arexx.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <stdlib.h>
#include <reaction/reaction_macros.h>
#include <stdio.h>

#include "input/input.h"
#include "MPlayer-arexx.h"
#include "mp_msg.h"

#undef USE_LOCAL_AREXX_GET

#ifdef USE_LOCAL_AREXX_GET
long rxid_get_sub_visibility();
long rxid_get_percent_pos();
long rxid_get_time_pos();
long rxid_get_time_length();
long rxid_get_vo_fullscreen();
#endif

extern struct Screen *My_Screen; //static
extern mp_cmd_t mp_cmds[]; //static

mp_cmd_t RxCmd;
long arexx_rc2_variable, arexx_paused;

char AREXXRESULT[10000];

void set_arexx_rc2(long v) 
{ 
	arexx_rc2_variable = v; 
}

long get_arexx_rc2() 
{ 
	return arexx_rc2_variable; 
}

static struct ARexxCmd rxCommands[] = 
{
     {"SEEK",               RXID_SEEK,               &rxFunc2, "FVALUE/K/A,TYPE/N", 0},
     {"SPEED_SET",          RXID_SPEED_SET,          &rxFunc1, "FVALUE/K/A", 0},
     {"SPEED_INCR",         RXID_SPEED_INCR,         &rxFunc1, "FVALUE/K/A", 0},
     {"SPEED_MULT",         RXID_SPEED_MULT,         &rxFunc1, "FVALUE/K/A", 0},
     {"EDL_MARK",           RXID_EDL_MARK,           &rxFunc0, NULL, 0},
     {"AUDIO_DELAY",        RXID_AUDIO_DELAY,        &rxFunc1, "FVALUE/K/A", 0},
     {"QUIT",               RXID_QUIT,               &rxFunc1, "VALUE/N", 0},
     {"PAUSE",              RXID_PAUSE,              &rxFunc0, NULL, 0},
     {"FRAME_STEP",         RXID_FRAME_STEP,         &rxFunc0, NULL, 0},
     {"GRAB_FRAMES",        RXID_GRAB_FRAMES,        &rxFunc0, NULL, 0},
     {"PT_STEP",            RXID_PT_STEP,            &rxFunc2sw, "VALUE/N/A,FORCE/S", 0},
     {"PT_UP_STEP",         RXID_PT_UP_STEP,         &rxFunc2sw, "VALUE/N/A,FORCE/S", 0},
     {"ALT_SRC_STEP",       RXID_ALT_SRC_STEP,       &rxFunc1, "VALUE/N/A", 0},
     {"SUB_DELAY",          RXID_SUB_DELAY,          &rxFunc2sw, "FVALUE/K/A,ABS/S", 0},
     {"SUB_STEP",           RXID_SUB_STEP,           &rxFunc1, "VALUE/N/A", 0},
     {"SUB_LOAD",           RXID_SUB_LOAD,           &rxFunc1, "SUBTITLE_FILE/K/A", 0},
     {"SUB_REMOVE",         RXID_SUB_REMOVE,         &rxFunc1, "VALUE/N", 0},
     {"OSD",                RXID_OSD,                &rxFunc1, "LEVEL/N", 0},
     {"OSD_SHOW_TEXT",      RXID_OSD_SHOW_TEXT,      &rxFunc1, "STRING/K/A", 0},
     {"VOLUME",             RXID_VOLUME,             &rxFunc2sw, "VALUE/N,ABS/S", 0},
     {"USE_MASTER",         RXID_USE_MASTER,         &rxFunc0, NULL, 0},
     {"MUTE",               RXID_MUTE,               &rxFunc0, NULL, 0},
     {"SWITCH_AUDIO",       RXID_SWITCH_AUDIO,       &rxFunc1, "VALUE/N/A", 0},
     {"CONTRAST",           RXID_CONTRAST,           &rxFunc2sw, "VALUE/N/A,ABS/S", 0},
     {"GAMMA",              RXID_GAMMA,              &rxFunc2sw, "VALUE/N/A,ABS/S", 0},
     {"BRIGHTNESS",         RXID_BRIGHTNESS,         &rxFunc2sw, "VALUE/N/A,ABS/S", 0},
     {"HUE",                RXID_HUE,                &rxFunc2sw, "VALUE/N/A,ABS/S", 0},
     {"SATURATION",         RXID_SATURATION,         &rxFunc2sw, "VALUE/N/A,ABS/S", 0},
     {"FRAME_DROP",         RXID_FRAME_DROP,         &rxFunc1, "VALUE/N", 0},
     {"SUB_POS",            RXID_SUB_POS,            &rxFunc1, "VALUE/N/A", 0},
     {"SUB_ALIGNMENT",      RXID_SUB_ALIGNMENT,      &rxFunc1, "VALUE/N", 0},
     {"SUB_VISIBILITY",     RXID_SUB_VISIBILITY,     &rxFunc0, NULL, 0},
     {"GET_SUB_VISIBILITY", RXID_GET_SUB_VISIBILITY, &rxFunc0, NULL, 0},
     {"SUB_SELECT",         RXID_SUB_SELECT,         &rxFunc1, "VALUE/N", 0},
     {"GET_PERCENT_POS",    RXID_GET_PERCENT_POS,    &rxFunc0, NULL, 0},
     {"GET_TIME_POS",       RXID_GET_TIME_POS,       &rxFunc0, NULL, 0},
     {"GET_TIME_LENGTH",    RXID_GET_TIME_LENGTH,    &rxFunc0, NULL, 0},
     {"VO_FULLSCREEN",      RXID_VO_FULLSCREEN,      &rxFunc0, NULL, 0},
     {"GET_VO_FULLSCREEN",  RXID_GET_VO_FULLSCREEN,  &rxFunc0, NULL, 0},
//     {"GET_SCREEN_PTR",     RXID_GET_SCREEN_PTR,     &rxFunc0, NULL, 0},
     {"VO_ONTOP",           RXID_VO_ONTOP,           &rxFunc0, NULL, 0},
     {"VO_ROOTWIN",         RXID_VO_ROOTWIN,         &rxFunc0, NULL, 0},
     {"SWITCH_VSYNC",       RXID_SWITCH_VSYNC,       &rxFunc1, "VALUE/N", 0},
     {"SWITCH_RATIO",       RXID_SWITCH_RATIO,       &rxFunc1, "VALUE/N", 0},
     {"PANSCAN",            RXID_PANSCAN,            &rxFunc2sw, "FVALUE/K/A,ABS/S", 0},
     {"LOADFILE",           RXID_LOADFILE,           &rxFunc2sw, "FILE=URL/K/A,APPEND/S", 0},
     {"LOADLIST",           RXID_LOADLIST,           &rxFunc2sw, "FILE/K/A,APPEND/S", 0},
     {"CHANGE_RECTANGLE",   RXID_CHANGE_RECTANGLE,   &rxFunc2, "VAL1/N/A,VAL2/N/A", 0},
     {"DVDNAV",             RXID_DVDNAV,             &rxFunc1, "BUTTON/N/A", 0},
//     {"DVDNAV_EVENT",       RXID_DVDNAV_EVENT,       &rxFunc1, "VALUE/N/A", 0},
     {"FORCED_SUBS_ONLY",   RXID_FORCED_SUBS_ONLY,   &rxFunc0, NULL, 0},
     {"DVB_SET_CHANNEL",    RXID_DVB_SET_CHANNEL,    &rxFunc2, "CHANNEL_NUMBER/N/A,CARD_NUMBER/N/A", 0},
     {"SCREENSHOT",         RXID_SCREENSHOT,         &rxFunc0, NULL, 0},
     {"MENU",               RXID_MENU,               &rxFunc1, "COMMAND/K/A", 0},
     {"SET_MENU",           RXID_SET_MENU,           &rxFunc1, "MENU_NAME/K/A", 0},
     {"HELP",               RXID_HELP,               &rxFunc0, NULL, 0},
     {"EXIT",               RXID_EXIT,               &rxFunc0, NULL, 0},
     {"HIDE",               RXID_HIDE,               &rxFunc0, NULL, 0},
     {NULL, 0, NULL, NULL, 0}
};

void arexx_help()
{
	for (struct ARexxCmd *item = rxCommands; item -> ac_Name ; item ++ )
	{
		sprintf(AREXXRESULT,"%s%s %s\n",AREXXRESULT,item->ac_Name, item->ac_ArgTemplate ? item->ac_ArgTemplate : "");
	}
}


static struct Task    *MainTask = NULL;           // Pointer to the main task;
struct Process *ARexx_Process = NULL;

static void ArexxTask (void)
{
	ULONG sigs;

	ArexxHandle *rxHandler = InitArexx();

	for(;;)
	{
		sigs = Wait( SIGBREAKF_CTRL_C | rxHandler->sigmask );
		if ( sigs & SIGBREAKF_CTRL_C ) break;

		if (rxHandler)
		{
			if(sigs & rxHandler->sigmask)
			{
				IDoMethod(rxHandler->rxObject,AM_HANDLEEVENT);
			}
		}
	}

	if (rxHandler)
	{
		EndArexx(rxHandler); 
	}
	Signal( (struct Task *) MainTask, SIGF_CHILD );
}

void StartArexx()
{
//	APTR output = Open("CON:",MODE_NEWFILE);

	MainTask = FindTask(NULL);
	ARexx_Process = CreateNewProcTags(
				NP_Name, "MPlayer ARexx Process" ,
				NP_Entry, ArexxTask, 
//				NP_Output, output,
				NP_Priority, 0,        
				NP_Child, TRUE,
				TAG_END);
}


void StopArexx()
{
	if (ARexx_Process)
	{
		Signal( &ARexx_Process->pr_Task, SIGBREAKF_CTRL_C );
		Wait(SIGF_CHILD);		// Wait for process to signal to continue
		Delay(1);				// I just wait and hope it has quit.
		ARexx_Process = NULL;
	}
}


ArexxHandle* InitArexx() 
{
	ArexxHandle *ro	= NULL;
	arexx_paused	= FALSE;

	if ((ro = (ArexxHandle*)AllocVec(sizeof(ArexxHandle),MEMF_SHARED|MEMF_CLEAR)))
	{
		if ((ro->ArexxBase = OpenLibrary("arexx.class",0)))
		{
			if ((ro->IARexx = (struct ARexxIFace*)GetInterface(ro->ArexxBase, "main", 1, NULL)))
			{
				// macors needs this!!
				IARexx = ro->IARexx;

				if (IIntuition)
				{
					ro->rxObject =  ARexxObject,
								AREXX_HostName, 	"MPLAYER",
								AREXX_NoSlot,		FALSE,
								AREXX_ReplyHook, 	NULL,
								AREXX_Commands, 	rxCommands,
								TAG_END);
				}
				else
				{
					ro->rxObject = NULL;
				}

				if(ro->rxObject) 
				{
					GetAttr(AREXX_SigMask,ro->rxObject,&(ro->sigmask));
					return ro;
				}
				else
			   		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Unable to initialize the AREXX Port (NewObject failed)\n");
				
				DropInterface((struct Interface*)ro->IARexx);
			}
			else
			   	mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Unable to initialize the AREXX Port (GetInterface failed)\n");

			CloseLibrary(ro->ArexxBase);
		}
		else
			mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Unable to initialize the AREXX Port (OpenLibrary failed)\n");
		
		FreeVec(ro);
		ro = NULL;
	}
	else
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Unable to initialize the AREXX Port (AllocVec failed)\n");
	
	return ro;
}

void EndArexx(ArexxHandle *ro) 
{
	if (ro) 
	{
		if (ro->rxObject) DisposeObject(ro->rxObject);
		if (ro->IARexx) DropInterface((struct Interface*)ro->IARexx);
		if (ro->ArexxBase) CloseLibrary(ro->ArexxBase);
		FreeVec(ro);
	}
}

void rxFunc0(struct ARexxCmd *cmd, struct RexxMsg *rm) 
{
	rxFunc(cmd,0,0,NULL,NULL,FALSE,FALSE);
}

void rxFunc1(struct ARexxCmd *cmd, struct RexxMsg *rm) 
{
	long value=0;
	char *str=NULL, isval=FALSE;
	if(cmd->ac_ArgList) 
	{
    	//We don't know if it's a string or number yet so cast to both char* and long.
		str =(char*)cmd->ac_ArgList[0];
		if(str) 
		{
			value=*(long*)str;
			isval=TRUE;
		}
	}
	rxFunc(cmd,value,0,str,NULL,isval,FALSE);
}

void rxFunc2(struct ARexxCmd *cmd, struct RexxMsg *rm) 
{
	long value=0, value2=0;
	char *str = NULL, *str2 = NULL, isval=FALSE, isval2=FALSE;
	if(cmd->ac_ArgList) 
	{
		//We don't know if it's a string or number yet so cast to both char* and long.
		str =(char*)cmd->ac_ArgList[0];
		str2=(char*)cmd->ac_ArgList[1];
		if(str) 
		{
			value=*(long*)str;
			isval=TRUE;
		}
		if(str2) 
		{
			value2=*(long*)str2;
			isval2=TRUE;
		}

		Printf("%ld, %ld\n", value, value2);
	}
	rxFunc(cmd,value,value2,str,str2,isval,isval2);
}

void rxFunc2sw(struct ARexxCmd *cmd, struct RexxMsg *rm) 
{
	long value=0;
	char *str = NULL, isval=FALSE, isval2=FALSE;
	if(cmd->ac_ArgList) 
	{
    	//We don't know if it's a string or number yet so cast to both char* and long.
		if ((str=(char*)cmd->ac_ArgList[0]))
		{
			value=*(long*)str;
			isval=TRUE;
		}
		isval2=cmd->ac_ArgList[1] != 0;

		Printf("value %ld\n", value);
	}
	rxFunc(cmd,value,isval2,str,NULL,isval,isval2);
}

void prepcommand(int cmd,int nargs) 
{
	RxCmd.id=cmd;
	RxCmd.name=NULL;
	RxCmd.nargs=nargs;
	RxCmd.args[nargs]=mp_cmds[cmd].args[nargs];
	RxCmd.pausing=arexx_paused;
}

void postcommand() 
{
	mp_cmd_t *c;
	if ((c=mp_cmd_clone(&RxCmd)))
    	mp_input_queue_cmd(c);
}

void put_command0(int cmd) 
{
	prepcommand(cmd,0);
	postcommand();
}

void put_icommand1(int cmd,int v) 
{
	prepcommand(cmd,1);
	RxCmd.args[0].type=MP_CMD_ARG_INT;
	RxCmd.args[0].v.i=v;
	postcommand();
}

void put_icommand2(int cmd,int v,int v2) 
{
	prepcommand(cmd,2);
	RxCmd.args[0].type=MP_CMD_ARG_INT;
	RxCmd.args[0].v.i=v;
	RxCmd.args[1].type=MP_CMD_ARG_INT;
	RxCmd.args[1].v.i=v2;
	postcommand();
}

void put_fcommand1(int cmd,float v) 
{
	prepcommand(cmd,1);
	RxCmd.args[0].type=MP_CMD_ARG_FLOAT;
	RxCmd.args[0].v.f=v;
	postcommand();
}

void put_fcommand2(int cmd,float v,int v2) 
{
	prepcommand(cmd,2);
	RxCmd.args[0].type=MP_CMD_ARG_FLOAT;
	RxCmd.args[0].v.f=v;
	RxCmd.args[1].type=MP_CMD_ARG_INT;
	RxCmd.args[1].v.i=v2;
	postcommand();
}

void put_scommand1(int cmd,char *v) 
{
	prepcommand(cmd,1);
	RxCmd.args[0].type=MP_CMD_ARG_STRING;
	RxCmd.args[0].v.s=v;
	postcommand();
}

void put_scommand2(int cmd,char *v,int v2) 
{
	prepcommand(cmd,2);
	RxCmd.args[0].type=MP_CMD_ARG_STRING;
	RxCmd.args[0].v.s=v;
	RxCmd.args[1].type=MP_CMD_ARG_INT;
	RxCmd.args[1].v.i=v2;
	postcommand();
}

void rxFunc(struct ARexxCmd *cmd, long value, long value2, char *str, char *str2, char isval, char isval2)
{
	Printf("ARexx: cmd=%s, v=%ld is=%ld, v2=%ld is2=%ld     \n",  cmd->ac_Name,value,isval,value2,isval2,str?str:"",str2?str2:"");

 	cmd->ac_RC=0; // No errors
	cmd->ac_RC2=0; 
	cmd -> ac_Result = AREXXRESULT;
	sprintf(AREXXRESULT,"");	// set default value



  switch(cmd->ac_ID)
  {
	case RXID_SEEK:
		//use value and isval2 and value2
		if(isval2)
			put_fcommand2(MP_CMD_SEEK,atoff(str),value2);
		else
			put_fcommand1(MP_CMD_SEEK,atoff(str));
		break;
	case RXID_SPEED_SET:      put_fcommand1(MP_CMD_SPEED_SET,atoff(str)); break; //use fval
	case RXID_SPEED_INCR:     put_fcommand1(MP_CMD_SPEED_INCR,atoff(str)); break; //use fval
	case RXID_SPEED_MULT:     put_fcommand1(MP_CMD_SPEED_MULT,atoff(str)); break; //use fval
#ifdef USE_EDL
	case RXID_EDL_MARK:       put_command0(MP_CMD_EDL_MARK); break;
#endif
	case RXID_AUDIO_DELAY:    put_fcommand1(MP_CMD_AUDIO_DELAY,atoff(str)); break; //use fval
	case RXID_QUIT:           put_command0(MP_CMD_QUIT); break; //use isval and value

	case RXID_PAUSE:
		arexx_paused=!arexx_paused;
		put_command0(MP_CMD_PAUSE);
		break;

	case RXID_FRAME_STEP:     put_command0(MP_CMD_FRAME_STEP); break;
	case RXID_GRAB_FRAMES:    put_command0(MP_CMD_GRAB_FRAMES); break;
	case RXID_PT_STEP:        put_icommand2(MP_CMD_PLAY_TREE_STEP,value,isval2); break; //use value and isval2
	case RXID_PT_UP_STEP:     put_icommand2(MP_CMD_PLAY_TREE_UP_STEP,value,isval2); break; //use value and isval2
	case RXID_ALT_SRC_STEP:   put_icommand1(MP_CMD_PLAY_ALT_SRC_STEP,value); break; //use value
	case RXID_SUB_DELAY:      put_fcommand2(MP_CMD_SUB_DELAY,atoff(str),isval2); break; //use fval and isval2
	case RXID_SUB_STEP:       put_icommand1(MP_CMD_SUB_STEP,value); break; //use value
	case RXID_SUB_LOAD:       put_scommand1(MP_CMD_SUB_LOAD,str); break; //use str
	case RXID_SUB_REMOVE:     put_icommand1(MP_CMD_SUB_REMOVE,isval?value:-1); break; //use isval and value
	case RXID_OSD:            put_icommand1(MP_CMD_OSD,isval?value:-1); break; //use isval and value
	case RXID_OSD_SHOW_TEXT:  put_scommand1(MP_CMD_OSD_SHOW_TEXT,str); break; //use str
	case RXID_VOLUME:       
						if (isval2)
						{
							put_fcommand2(MP_CMD_VOLUME, (float) value,isval2); 
						}
						else
						{
							put_icommand2(MP_CMD_VOLUME, value,isval2); 
						}
						break; //use value and isval2

	case RXID_MUTE:           put_command0(MP_CMD_MUTE); break;
	case RXID_SWITCH_AUDIO:   put_icommand1(MP_CMD_SWITCH_AUDIO,isval?value:-1); break; //use isval and value
	case RXID_CONTRAST:       put_icommand2(MP_CMD_CONTRAST,value,isval2); break; //use value and isval2
	case RXID_GAMMA:          put_icommand2(MP_CMD_GAMMA,value,isval2); break; //use value and isval2
	case RXID_BRIGHTNESS:     put_icommand2(MP_CMD_BRIGHTNESS,value,isval2); break; //use value and isval2
	case RXID_HUE:            put_icommand2(MP_CMD_HUE,value,isval2); break; //use value and isval2
	case RXID_SATURATION:     put_icommand2(MP_CMD_SATURATION,value,isval2); break; //use value and isval2
	case RXID_FRAME_DROP:     put_icommand1(MP_CMD_FRAMEDROPPING,isval?value:-1); break; //use isval and value
	case RXID_SUB_POS:        put_icommand1(MP_CMD_SUB_POS,value); break; //use value
	case RXID_SUB_ALIGNMENT:  put_icommand1(MP_CMD_SUB_ALIGNMENT,isval?value:-1); break; //use value
	case RXID_SUB_VISIBILITY: put_command0(MP_CMD_SUB_VISIBILITY); break;
	case RXID_SUB_SELECT:     put_icommand1(MP_CMD_SUB_SELECT,isval?value:-1); break; //use isval and value
	case RXID_GET_SUB_VISIBILITY:
#ifdef USE_LOCAL_AREXX_GET
		cmd->ac_RC2=rxid_get_sub_visibility();
#else
		put_command0(MP_CMD_GET_SUB_VISIBILITY);
		sprintf(AREXXRESULT,"%d",get_arexx_rc2());
#endif
		break;
	case RXID_GET_PERCENT_POS:
#ifdef USE_LOCAL_AREXX_GET
		sprintf(AREXXRESULT,"%d",rxid_get_percent_pos());
#else
		put_command0(MP_CMD_GET_PERCENT_POS);
		sprintf(AREXXRESULT,"%d",get_arexx_rc2());
#endif
		break;
	case RXID_GET_TIME_POS:
#ifdef USE_LOCAL_AREXX_GET
		sprintf(AREXXRESULT,"%d",rxid_get_time_pos());
#else
		put_command0(MP_CMD_GET_TIME_POS);
		sprintf(AREXXRESULT,"%d",get_arexx_rc2());
#endif
		break;
	case RXID_GET_TIME_LENGTH:
#ifdef USE_LOCAL_AREXX_GET
		sprintf(AREXXRESULT,rxid_get_time_length());
#else
		put_command0(MP_CMD_GET_TIME_LENGTH);
		sprintf(AREXXRESULT,"%d",get_arexx_rc2());
#endif
		break;
	case RXID_GET_VO_FULLSCREEN:
#ifdef USE_LOCAL_AREXX_GET
		sprintf(AREXXRESULT,"%d",rxid_get_vo_fullscreen());
#else
		put_command0(MP_CMD_GET_VO_FULLSCREEN);
		sprintf(AREXXRESULT,"%d",get_arexx_rc2());
#endif
		break;

	case RXID_VO_FULLSCREEN: put_command0(MP_CMD_VO_FULLSCREEN); break;
	case RXID_VO_ONTOP:      put_command0(MP_CMD_VO_ONTOP); break;
	case RXID_VO_ROOTWIN:    put_command0(MP_CMD_VO_ROOTWIN); break;
	case RXID_SWITCH_VSYNC:
		//use isval and value
		if(isval)
			put_icommand1(MP_CMD_SWITCH_VSYNC,value);
		else
			put_command0(MP_CMD_SWITCH_VSYNC);
		break;
	case RXID_SWITCH_RATIO:
    		//use isval and str
		if(isval)
			put_fcommand1(MP_CMD_SWITCH_RATIO,atoff(str));
		else
			put_command0(MP_CMD_SWITCH_RATIO);
		break;
	case RXID_PANSCAN:            put_fcommand2(MP_CMD_PANSCAN,atoff(str),isval2); break; //use value and value2
	case RXID_LOADFILE:           put_scommand2(MP_CMD_LOADFILE,str,isval2); break; //use str and isval2
	case RXID_LOADLIST:           put_scommand2(MP_CMD_LOADLIST,str,isval2); break; //use str and isval2
	case RXID_CHANGE_RECTANGLE:   put_icommand2(MP_CMD_VF_CHANGE_RECTANGLE,value,value2); break; //use value and value2
	case RXID_DVDNAV:             put_icommand1(MP_CMD_DVDNAV,value); break; //use value
//	case RXID_DVDNAV_EVENT:       put_icommand1(MP_CMD_DVDNAV_EVENT,value); break; //use value
	case RXID_FORCED_SUBS_ONLY:   put_command0(MP_CMD_SUB_FORCED_ONLY); break;
#ifdef HAS_DVBIN_SUPPORT
	case RXID_DVB_SET_CHANNEL:    put_icommand2(MP_CMD_DVB_SET_CHANNEL,value,value2); break; //use value and value2
#endif
	case RXID_SCREENSHOT:         put_command0(MP_CMD_SCREENSHOT); break;
	case RXID_USE_MASTER:         put_command0(MP_CMD_MIXER_USEMASTER); break;
	case RXID_MENU:               put_scommand1(MP_CMD_MENU,str); break; //use str
	case RXID_SET_MENU:           put_scommand1(MP_CMD_SET_MENU,str); break; //use str
	case RXID_HELP:

					// put_command0(MP_CMD_CHELP); 
					arexx_help();

					break;
	case RXID_EXIT:               put_command0(MP_CMD_CEXIT); break;
	case RXID_HIDE:               put_command0(MP_CMD_CHIDE); break;

	default:
		cmd -> ac_RC = -3;		// Unknown ARexx command
  }

	Printf("RC: %ld,%ld,'%s'\n",cmd ->ac_RC,cmd ->ac_RC2, cmd -> ac_Result);

}
