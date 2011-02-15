#include <exec/types.h>
#include <proto/timer.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/asl.h>
#include <proto/wb.h>
#include <proto/icon.h>

/* ARexx */
#include "MPlayer-arexx.h"
/* ARexx */

#ifndef __AMIGAOS4__
#include <proto/alib.h>
#else
#include <clib/alib_protos.h>
#endif
#include <proto/icon.h>
#include <proto/dos.h>
#include <proto/wb.h>
#include <proto/application.h>
#include <libraries/application.h>

#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <workbench/startup.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "version.h"
#include "mp_msg.h"
#include "amigaos_stuff.h"

const char version[] = "$VER: MPlayer for AmigaOS4 " VERSION ;
const char STACK[] __attribute((used))   = "$STACK: 5000000";

char *SCREENSHOTDIR = "RAM:";
char *EXTPATTERN = "(#?.avi|#?.mpg|#?.mpeg|#?.asf|#?.wmv|#?.vob|#?.rm|#?.mov|#?.mp3|#?.ogg|#?.wav|#?.wmv|#?.wma|#?.flv|#?.asf|#?.mp4)";

struct Process *p = NULL; 	//this to remove
APTR OldPtr =NULL;			//the requesters

#ifdef USE_ASYNCIO
struct Library 			 *AsyncIOBase = NULL;
struct AsyncIOIFace 	 *IAsyncIO=NULL;
#endif

struct Device 			 *TimerBase = NULL;
//extern struct TimerIFace *ITimer;
struct Library 			 *AslBase = NULL;
struct AslIFace 		 *IAsl = NULL;
struct Library 			 *ApplicationBase = NULL;
struct ApplicationIFace  *IApplication = NULL;

UBYTE  TimerDevice = -1; // -1 -> not opened
struct TimeRequest 		 *TimerRequest = NULL;
struct MsgPort 	   		 *TimerMsgPort = NULL;

struct MsgPort   		 *AppPort = NULL;

/* ARexx */
struct ArexxHandle 		 *rxHandler = NULL;
/* ARexx */

static char **AmigaOS_argv = NULL;
static int AmigaOS_argc = 0;
uint32 AppID = 0;

static void Free_Arg(void);
extern void RemoveAppPort(void);
extern struct DiskObject *DIcon;
extern struct AppIcon 	 *MPLAppIcon;

#define GET_PATH(drawer,file,dest)                                                      \
	dest = (char *) malloc( ( strlen(drawer) + strlen(file) + 2 ) * sizeof(char) );     \
	if (dest)																																						\
		{                                                                         					\
				if ( strlen(drawer) == 0) strcpy(dest, file);																	\
				else																																						\
				{																																								\
			if ( (drawer[ strlen(drawer) - 1  ] == ':')  ) sprintf(dest, "%s%s", drawer, file);  \
			else sprintf(dest, "%s/%s", drawer, file);																		\
				}																																								\
		}

/******************************/

static void Free_Arg(void)
{
	if (AmigaOS_argv)
	{
		ULONG i;
		for (i=0; i < AmigaOS_argc; i++)
			if(AmigaOS_argv[i]) free(AmigaOS_argv[i]);

		free(AmigaOS_argv);
	}
}


/******************************/
void AmigaOS_ParseArg(int argc, char *argv[], int *new_argc, char ***new_argv)
{
	struct WBStartup *WBStartup = NULL;
	static TEXT progname[512];
	BOOL gotprogname = FALSE;
	struct DiskObject *icon = NULL;

/* Summer Edition Reloaded */
	/* Check for Altivec.. in case an altivec version is loaded on SAM or on a G3.. */
	#if HAVE_ALTIVEC_H == 1
	ULONG result = 0;

	GetCPUInfoTags(GCIT_VectorUnit, &result, TAG_DONE);
	if (result != VECTORTYPE_ALTIVEC)
	{
		mp_msg(MSGT_VO, MSGL_ERR, "Sorry, this version is only for Altivec capable machine!");
		AmigaOS_Close();
		return;
	}
	#endif

	/* End Check */
/* Summer Edition Reloaded */

	*new_argc = argc;
	*new_argv = argv;

	// Ok is launch from cmd line, just do nothing
	if (argc)
	{
		return;
	}
	// Ok, ran from WB, we have to handle some funny thing now
	// 1. if there is some WBStartup->Arg then play that file and go
	// 2. else open an ASL requester and add the selected file

	// 1. WBStartup
	WBStartup = (struct WBStartup *) argv;
	if (!WBStartup)
	{
		return ; // We never know !
	}

	if (Cli())
	{
		static char temp[8192];

		if (GetProgramName(temp, sizeof(temp)))
		{
			strcpy(progname, "PROGDIR:");
			strlcat(progname, FilePart(temp), sizeof(progname));
			gotprogname = TRUE;
		}
	}
	else
	{
		strlcpy(progname, FindTask(NULL)->tc_Node.ln_Name, sizeof(progname));
		gotprogname = TRUE;
	}

	icon = gotprogname ? GetDiskObject(progname) : NULL;
	if (icon)
	{
		STRPTR found;

		found = FindToolType(icon->do_ToolTypes, "SCREENSHOTDIR");
		if (found)
		{
			SCREENSHOTDIR = strdup(found);
		}
		found = FindToolType(icon->do_ToolTypes, "EXTPATTERN");
		if (found)
		{
			EXTPATTERN = strdup(found);
		}

		FreeDiskObject(icon);
	}

	if (WBStartup->sm_NumArgs > 1)
	{
		// The first arg is always the tool name (aka us)
		// Then if more than one arg, with have some file name
		ULONG i;

		// We will replace the original argc/argv by us
		AmigaOS_argc = WBStartup->sm_NumArgs;
		AmigaOS_argv = malloc(AmigaOS_argc * sizeof(char *) );
		if (!AmigaOS_argv) goto fail;

		memset(AmigaOS_argv, 0x00, AmigaOS_argc * sizeof(char *) );

		for( i=0; i < AmigaOS_argc; i++)
		{
			AmigaOS_argv[i] = malloc(strlen(WBStartup->sm_ArgList[i].wa_Name) + 1);
			if (!AmigaOS_argv[i])
			{
				goto fail;
			}
			strcpy(AmigaOS_argv[i], WBStartup->sm_ArgList[i].wa_Name);
		}

		*new_argc = AmigaOS_argc;
		*new_argv = AmigaOS_argv;
	}
	else
	{
		ULONG i=0;
		struct FileRequester * AmigaOS_FileRequester = NULL;
		BPTR FavoritePath_File;
		char FavoritePath_Value[1024];
		BOOL FavoritePath_Ok = FALSE;

		FavoritePath_File = Open("PROGDIR:FavoritePath", MODE_OLDFILE);
		if (FavoritePath_File)
		{
			LONG size =Read(FavoritePath_File, FavoritePath_Value, 1024);
			if (size > 0)
			{
				if ( strchr(FavoritePath_Value, '\n') ) // There is an /n -> valid file
				{
					FavoritePath_Ok = TRUE;
					*(strchr(FavoritePath_Value, '\n')) = '\0';
				}
			}
			Close(FavoritePath_File);
		}

		AmigaOS_FileRequester = AllocAslRequest(ASL_FileRequest, NULL);
		if (!AmigaOS_FileRequester)
		{
			mp_msg(MSGT_CPLAYER, MSGL_FATAL,"Cannot open FileRequester!\n");
			goto fail;
		}

		if ( ( AslRequestTags( AmigaOS_FileRequester,
					ASLFR_TitleText,        "Choose some videos",
					ASLFR_DoMultiSelect,    TRUE,
					ASLFR_RejectIcons,      TRUE,
					ASLFR_DoPatterns,		TRUE,
					ASLFR_InitialPattern,	(ULONG)EXTPATTERN,
		    		ASLFR_InitialDrawer,    (FavoritePath_Ok) ? FavoritePath_Value : "",
					TAG_DONE) ) == FALSE )
		{
			FreeAslRequest(AmigaOS_FileRequester);
			AmigaOS_FileRequester = NULL;
			mp_msg(MSGT_CPLAYER, MSGL_FATAL,"Fail AslRequestTags!\n");
	  		goto fail;
		}

		AmigaOS_argc = AmigaOS_FileRequester->fr_NumArgs + 1;
		AmigaOS_argv = malloc(AmigaOS_argc * sizeof(char *) );
		if (!AmigaOS_argv) goto fail;

		memset(AmigaOS_argv, 0x00, AmigaOS_argc * sizeof(char *) );

		AmigaOS_argv[0] = strdup("mplayer");
		if (!AmigaOS_argv[0]) goto fail;

		for( i=1; i < AmigaOS_argc; i++)
		{
			GET_PATH(AmigaOS_FileRequester->fr_Drawer,
						AmigaOS_FileRequester->fr_ArgList[i-1].wa_Name,
						AmigaOS_argv[i]);
			if (!AmigaOS_argv[i])
			{
				FreeAslRequest(AmigaOS_FileRequester);
				AmigaOS_FileRequester = NULL;
				goto fail;
			}
		}

		*new_argc = AmigaOS_argc;
		*new_argv = AmigaOS_argv;

		FreeAslRequest(AmigaOS_FileRequester);
	}

	return;

fail:
	Free_Arg();

	*new_argc = argc;
	*new_argv = argv;
	return;
}


/******************************/
/******************************/
void AmigaOS_Close(void)
{
	if (OldPtr)
	{
		//SetTaskPri(p,0);
		SetProcWindow((APTR) OldPtr);
	}
	//if (t) t=NULL;

	RemoveAppPort();

	if (MPLAppIcon)	RemoveAppIcon(MPLAppIcon);
    if (DIcon)		FreeDiskObject(DIcon);

/* ARexx */
    if (NULL != rxHandler) EndArexx(rxHandler);
/* ARexx */

	if (AppID>0)
	{
		SetApplicationAttrs(AppID, APPATTR_AllowsBlanker, TRUE, TAG_DONE);
		SetApplicationAttrs(AppID, APPATTR_NeedsGameMode, FALSE, TAG_DONE);
		//SendApplicationMsg(AppID, 0, NULL, APPLIBMT_BlankerAllow);
		UnregisterApplication(AppID, NULL);
	}
	Free_Arg();


#ifdef USE_ASYNCIO
	if (AsyncIOBase) CloseLibrary( AsyncIOBase ); AsyncIOBase = 0;
	if (IAsyncIO) DropInterface((struct Interface*)IAsyncIO); IAsyncIO = 0;
#endif

	if (ApplicationBase) CloseLibrary(ApplicationBase); ApplicationBase = 0;
	if (IApplication) DropInterface((struct Interface*)IApplication); IApplication = 0;

	if (AslBase) CloseLibrary(AslBase); AslBase = 0;
	if (IAsl) DropInterface((struct Interface*)IAsl); IAsl = 0;

	if (!TimerDevice) CloseDevice( (struct IORequest *) TimerDevice); TimerDevice = 0;
	if (TimerRequest) FreeSysObject ( ASOT_IOREQUEST, TimerRequest ); TimerRequest = 0;
	if (TimerMsgPort) FreeSysObject ( ASOT_PORT, TimerMsgPort ); TimerMsgPort = 0;
	if (ITimer) DropInterface((struct Interface*) ITimer); ITimer = 0;
}

/******************************/
/******************************/
int AmigaOS_Open(int argc, char *argv[])
{
	AmigaOS_argv 					= NULL;

	//Remove requesters
	//OldPtr = SetProcWindow((APTR) -1L);

	if (!argc)
	{
		// If argc == 0 -> execute from the WB, then no need to display anything
		freopen("NIL:", "w", stderr);
		freopen("NIL:", "w", stdout);
	}

	AppPort = AllocSysObject(ASOT_PORT,NULL);
	if (!AppPort)
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Failed to create AppPort\n");
		return -1;
	}

	TimerMsgPort = AllocSysObject(ASOT_PORT,NULL);
	if ( !TimerMsgPort )
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Failed to create MsgPort\n");
		return -1;
	}

	TimerRequest = AllocSysObjectTags(ASOT_IOREQUEST,
									  ASOIOR_Size, sizeof(struct TimeRequest),
									  ASOIOR_ReplyPort, TimerMsgPort,
									  TAG_DONE);
	if ( !TimerRequest )
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Failed to create IORequest\n");
		return -1;
	}

	if ( (TimerDevice = OpenDevice((unsigned char*)TIMERNAME, UNIT_MICROHZ , (struct IORequest *) TimerRequest, 0) ) )
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Failed to open" TIMERNAME ".\n");
		return -1;
	}

	TimerBase = (struct Device *) TimerRequest->Request.io_Device;

	ITimer = (struct TimerIFace*) GetInterface( (struct Library*)TimerBase, (unsigned char*)"main", 1, NULL );

#ifdef USE_ASYNCIO
	if ( ! ( AsyncIOBase = OpenLibrary( (unsigned char*)"asyncio.library", 0L) ) )
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Unable to open the asyncio.library\n");
		return -1;
	}

	if (!(IAsyncIO = (struct AsyncIOIFace *)GetInterface((struct Library *)AsyncIOBase,(unsigned char*)"main",1,NULL)))
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Failed to Get AsyncIO Interface.\n");
		return -1;
	}
#endif

	if ( ! ( AslBase = OpenLibrary( (unsigned char*)"asl.library", 0L) ) )
	{
   		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Unable to open the asl.library\n");
		return -1;
	}
	if (!(IAsl = (struct AslIFace *)GetInterface((struct Library *)AslBase,(unsigned char*)"main",1,NULL)))
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Failed to Get ASL Interface.\n");
		return -1;
	}


	ApplicationBase = OpenLibrary( (unsigned char*)"application.library", 51L);
	if (!ApplicationBase)
	{
   		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Unable to open the application.library 51!\n");
		return -1;
	}

	IApplication = (struct ApplicationIFace *)GetInterface((struct Library *)ApplicationBase,(unsigned char*)"application",1,NULL);
	if (!IApplication)
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Failed to Get Application Interface.\n");
		return -1;
	}

	//Register the application so the ScreenBlanker don't bother us..
	AppID = RegisterApplication((unsigned char*)"MPlayer",
			REGAPP_URLIdentifier, 		"amigasoft.net",
			REGAPP_LoadPrefs, 			TRUE,
			REGAPP_SavePrefs, 			TRUE,
			REGAPP_ENVDir, 				"PROGDIR:",
			REGAPP_NoIcon,				TRUE,
			REGAPP_UniqueApplication, 	TRUE,
			REGAPP_AllowsBlanker,		FALSE,
			TAG_DONE);

	if (AppID == 0)
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Failed to Register Application.\n");
		AmigaOS_Close();
		return -1;
	}

	SetApplicationAttrs(AppID, APPATTR_AllowsBlanker, FALSE, TAG_DONE);
	//SendApplicationMsg(AppID, 0, NULL, APPLIBMT_BlankerDisallow);

/* ARexx */
	if ( ! ( rxHandler = InitArexx() ) )
	{
   		mp_msg(MSGT_CPLAYER, MSGL_FATAL, "Unable to open the arexx port (it requires arexx.class)\n");
		AmigaOS_Close();
		return -1;
	}
/* ARexx */

	return 0;
}
