
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/DOS.h>
#include <proto/ASL.h>
#include <proto/requester.h>
#include <proto/Exec.h>
#include <classes/requester.h>
#include <libraries/gadtools.h>

#include "mp_msg.h"
#include "mp_core.h"

#include "input/input.h"
#include "arexx.h"
#include "menu.h"
#include "../osdep/keycodes.h"
#include "../version.h"

extern mp_cmd_t mp_cmds[]; //static

extern struct Screen *My_Screen;

extern int stream_cache_size;	// Need to set this to 0 if you select DVDNAV.

void ShowAbout(void);

static void OpenDVD(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED);
static void OpenDVDNAV(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED);
static void OpenVCD(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED);
static void OpenNetwork(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED);

// not static used in drag and drop.

int32 PlayFile_async(const char *FileName);

// only used for menus

static UBYTE *LoadFile(const char *StartingDir);
static int32 PrintMsgProc(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase);
void PrintMsg(CONST_STRPTR text, int REQ_TYPE, int REQ_IMAGE);

extern void make_appwindow(struct Window *win );
extern void delete_appwindow();

void prevfilm();
void nextfilm();

BOOL loop_on();

extern BOOL choosing_flag;
extern set_gfx_rendering_option();

void choosing(BOOL set)
{
	choosing_flag = set;
	set_gfx_rendering_option();
}

extern char *EXTPATTERN;
//extern MPContext *mpctx;

enum
{
	ID_load_file,
	ID_open_dvd,
	ID_open_dvdnav,
	ID_open_vcd,
	ID_open_network,
	ID_about = 6,
	ID_quit = 8
};

enum
{
	ID_play,
	ID_loop,
	ID_prev,
	ID_next
};

enum
{
	ID_mute,
	ID_volume_up,
	ID_volume_down
};

void open_menu(void);

void cmd_load_file(void);
void cmd_open_dvd(void);
void cmd_open_dvdnav(void);
void cmd_open_vcd(void);
void cmd_open_network(void);
void cmd_play(void);

void menu_mplayer(ULONG menucode);
void menu_play(ULONG menucode);
void menu_video(ULONG menucode);
void menu_aspect(ULONG menucode);
void menu_audio(ULONG menucode);

void (*select_menu[]) (ULONG menucode) =
{
	menu_mplayer,
	menu_play,
	menu_video,
//	menu_aspect,
	menu_audio
};

struct pmpMessage {
	char *about;
	ULONG type;
	ULONG image;
};

struct pmpMessage errmsg =
{
	"Error",0,0
};

	int play_start_id = 10;
	int video_start_id = 14;

int spawn_count = 0;

/****************** MENU DEFINITION *****************/
    static struct Menu *menu;

/*
struct NewMenu amiga_menu[] =
{
    { NM_TITLE, "MPlayer ", 0, 0, 0, 0},
    { NM_ITEM, "Load File..", 0, 0, 0, 0},
    { NM_ITEM, "Open DVD", "O", 0, 0, 0},
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0},
    { NM_ITEM, "Save", "S", 0, 0, 0},
    { NM_ITEM, "Save as...", 0, 0, 0, 0},
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0},
    { NM_ITEM, "About...", "?", 0, 0, 0},
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0},
    { NM_ITEM, "Quit...", "Q", 0, 0, 0},

    { NM_TITLE, "Prefs ", 0, 0, 0, 0},
    { NM_ITEM, "Use Extras", 0, MENUTOGGLE | CHECKIT | CHECKED, 0, 0},    //  1
    { NM_ITEM, "Use SubMenus", 0, MENUTOGGLE | CHECKIT | CHECKED, 0, 0},  //  2
    { NM_ITEM, "Use Radios", 0, MENUTOGGLE | CHECKIT | CHECKED, 0, 0},    //  4
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0},                                  //  8
    { NM_ITEM, "Radio 1", 0, CHECKIT | CHECKED, 96, 0},                   //  16
    { NM_ITEM, "Radio 2", 0, CHECKIT, 80, 0},                             //  32
    { NM_ITEM, "Radio 3", 0, CHECKIT, 48, 0},                             //  64
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0},                                  //  128
    { NM_ITEM, "SubItems", 0, 0, 0, 0},                                   //  256

        { NM_SUB, "Enable Radios", 0, MENUTOGGLE | CHECKIT, 32, 0},             //  1
        { NM_SUB, NM_BARLABEL, 0, 0, 0, 0},                                     //  2
        { NM_SUB, "Radio 4", 0, NM_ITEMDISABLED | CHECKIT | CHECKED, 8, 0},     //  4
        { NM_SUB, "Radio 5", 0, NM_ITEMDISABLED | CHECKIT, 4, 0},               //  8
        { NM_SUB, NM_BARLABEL, 0, 0, 0, 0},                                     //  16
        { NM_SUB, "Disable Radios", 0, MENUTOGGLE | CHECKIT | CHECKED, 1, 0},   //  32

    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0},                                  //  512
    { NM_ITEM, "Save Prefs", 0, 0, 0, 0},                                 //  1024


    { NM_TITLE, "Extras ", 0, 0, 0, 0},
    { NM_ITEM, "Help", 0, 0, 0, 0},
    { NM_ITEM, "Arexx", 0, 0, 0, 0},

    {  NM_END, NULL, 0, 0, 0, 0}
};
*/


struct NewMenu amiga_menu[] =
{
      // Type, Label, CommKey, Flags, MutualExclude, UserData 

	{ NM_TITLE, (STRPTR)"MPlayer",                    NULL,   0, 0L, NULL },
		{ NM_ITEM,  (STRPTR)"Load File..",            (STRPTR)"L",    0, 0L, NULL },
		{ NM_ITEM,  (STRPTR)"Open DVD",            		0,    0, 0L, NULL },
		{ NM_ITEM,  (STRPTR)"Open DVDNAV",          	(STRPTR)"D",    0, 0L, NULL },
		{ NM_ITEM,  (STRPTR)"Open VCD",            	(STRPTR)"V",    0, 0L, NULL },
		{ NM_ITEM,  (STRPTR)"Open Network",          	(STRPTR)"N",    0, 0L, NULL },
		{ NM_ITEM, NM_BARLABEL,				0, 0, 0, 0},
		{ NM_ITEM,  (STRPTR)"About MPlayer",        	(STRPTR)"A",    0, 0L, NULL },
		{ NM_ITEM, NM_BARLABEL,				0, 0, 0, 0},
		{ NM_ITEM,  (STRPTR)"Quit",                   (STRPTR)"Q",    0, 0L, NULL },

	{ NM_TITLE, (STRPTR)"Play",                       NULL,   0, 0L, NULL },
		{ NM_ITEM,  (STRPTR)"Play/Pause",             (STRPTR)"P",    0, 0L, NULL },
		{ NM_ITEM,  (STRPTR)"Loop",                   (STRPTR)"L",    MENUTOGGLE|CHECKIT,  0L, NULL },
//		{ NM_ITEM,  (STRPTR)"Stop",                   (STRPTR)"S",    0, 0L, NULL },
		{ NM_ITEM,  (STRPTR)"Prev film",			0,    0, 0L, NULL },
		{ NM_ITEM,  (STRPTR)"Next film",			0,    0, 0L, NULL },


      { NM_TITLE, (STRPTR)"Video Options",              NULL,   0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Stay on Top",            NULL,   MENUTOGGLE|CHECKIT , 0L, NULL },
          { NM_ITEM,  (STRPTR)"Cycle subtitles",         0,    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Fullscreen",             	0,    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Screenshot",             NULL,   0, 0L, NULL },
/*
          { NM_ITEM,  NM_BARLABEL,              NULL,	0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Normal Dimension",       (STRPTR)"1",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Doubled Dimension",      (STRPTR)"2",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Screen Dimension",       (STRPTR)"3",    0, 0L, NULL },
          { NM_ITEM,  (STRPTR)"Half Dimension",         (STRPTR)"4",    0, 0L, NULL },
          { NM_ITEM,  NM_BARLABEL,              NULL,	0, 0L, NULL },
// 23
	 { NM_ITEM,  (STRPTR)"Aspect Ratio",       	NULL,   0, 0L, NULL }, 
		  { NM_SUB,   (STRPTR)"Original",      			NULL,   MENUTOGGLE|CHECKIT|CHECKED, ~1, NULL },
          { NM_SUB,   (STRPTR)"16:10",      			NULL,   MENUTOGGLE|CHECKIT, ~2, NULL },
          { NM_SUB,   (STRPTR)"16:9",      				NULL,   MENUTOGGLE|CHECKIT, ~4, NULL },
          { NM_SUB,   (STRPTR)"1.85:1",      			NULL,   MENUTOGGLE|CHECKIT, ~8, NULL },
          { NM_SUB,   (STRPTR)"2.21:1",      			NULL,   MENUTOGGLE|CHECKIT, ~16, NULL },
          { NM_SUB,   (STRPTR)"2.35:1",      			NULL,   MENUTOGGLE|CHECKIT, ~32, NULL },
          { NM_SUB,   (STRPTR)"2.39:1",      			NULL,   MENUTOGGLE|CHECKIT, ~64, NULL },
          { NM_SUB,   (STRPTR)"5:3",      				NULL,   MENUTOGGLE|CHECKIT, ~128, NULL },
          { NM_SUB,   (STRPTR)"4:3",      				NULL,   MENUTOGGLE|CHECKIT, ~256, NULL },
          { NM_SUB,   (STRPTR)"5:4",      				NULL,   MENUTOGGLE|CHECKIT, ~512, NULL },
          { NM_SUB,   (STRPTR)"1:1",      				NULL,   MENUTOGGLE|CHECKIT, ~1024, NULL },
*/

      { NM_TITLE, (STRPTR)"Audio Options",              NULL,   0, 0L, NULL },
		{ NM_ITEM,  (STRPTR)"Mute",                   NULL,   MENUTOGGLE|CHECKIT , 0, NULL },
		{ NM_ITEM,  (STRPTR)"Volume Up"     ,         (STRPTR)"+",    0, 0L, NULL },
		{ NM_ITEM,  (STRPTR)"Volume Down",            (STRPTR)"-",    0, 0L, NULL },

      { NM_END,   NULL,                         NULL,   0, 0L, NULL }
    };

struct Window *Menu_Window;

void attach_menu(struct Window *window)
{
	struct VisualInfo *vi;
	open_menu();

	if ((menu)&&(window))
	{
#if 1
		make_appwindow(window);
#endif

		if (vi = GetVisualInfoA(window -> WScreen,NULL))
		{
			LayoutMenus( menu, vi, GTMN_NewLookMenus, TRUE, TAG_END );
		}

		SetMenuStrip(window, menu);
		Menu_Window = window;
	}
}



void detach_menu(struct Window *window)
{
#if 1
	delete_appwindow();
#endif

	if (menu)
	{
		while (spawn_count)
		{
			Printf("You need to close requester windows\ncan't remove menu from mplayer, yet\n");
			Delay(1);
		}

		ClearMenuStrip(window);
		FreeMenus( menu );
		menu = NULL;
	}
}

void open_menu(void)
{

	if (loop_on()==TRUE)
		amiga_menu[ play_start_id + ID_loop ].nm_Flags = MENUTOGGLE|CHECKIT|CHECKED;
	else
		amiga_menu[ play_start_id + ID_loop ].nm_Flags = MENUTOGGLE|CHECKIT;

/*
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
*/

	menu = CreateMenusA(amiga_menu,NULL);
}

void spawn_died(int32 ret,int32 x)
{
	spawn_count--;
	printf(" spawn count %d\n",spawn_count );
}

struct Process *spawn( void *fn, char *name )
{
	struct Process *ret = (struct Process *) CreateNewProcTags (
	   			NP_Entry, 		(ULONG) fn,
				NP_Name,	   	name,
				NP_StackSize,   262144,
				NP_Child,		TRUE,
				NP_Priority,	0,
				NP_ExitData, 	IDOS,
				NP_FinalCode, spawn_died,
				TAG_DONE);

	if (ret) spawn_count ++;

	printf(" spawn count %d\n",spawn_count );

	return ret;
}

char *__tmp_file_name = NULL;


void load_file_proc(STRPTR args, int32 length, APTR execbase)
{
	char *FileName;

	if (FileName = LoadFile(NULL))
	{
		__tmp_file_name = strdup(FileName);
		PlayFile_proc(args,length,execbase);
	}
}


void cmd_load_file()
{
	struct Process *proc;

	if (choosing_flag == FALSE)
	{
		proc = spawn( load_file_proc, 	"load file" );
		if (!proc) PrintMsg(&errmsg,REQTYPE_INFO,REQIMAGE_ERROR);
	}
}

void PlayFile_proc(STRPTR args, int32 length, APTR execbase)
{
	if (__tmp_file_name)
	{
		seek_start();
		Delay(8);

		add_file_to_que( __tmp_file_name );
		Delay(8);

		nextfilm();

		free(__tmp_file_name);
		__tmp_file_name = NULL;
	}

	return RETURN_OK;
}


int32 PlayFile_async(const char *FileName)
{
	struct Process *proc;
	if (choosing_flag == FALSE)
	{
		__tmp_file_name = strdup(FileName);
		proc = spawn( PlayFile_proc,  "mplayer:async" );
		if (!proc) PrintMsg(&errmsg,REQTYPE_INFO,REQIMAGE_ERROR);
	}

	return RETURN_OK;
}



void cmd_open_dvd()
{
	struct Process *proc;
	if (choosing_flag == FALSE)
	{
		proc = spawn( OpenDVD, 	"Load DVD" );
		if (!proc) PrintMsg(&errmsg,REQTYPE_INFO,REQIMAGE_ERROR);
	}
}

void cmd_open_dvdnav()
{
	struct Process *proc;
	if (choosing_flag == FALSE)
	{
		proc = spawn( OpenDVDNAV, 	"Load DVDNAV" );
		if (!proc) PrintMsg(&errmsg,REQTYPE_INFO,REQIMAGE_ERROR);
	}
}

void cmd_open_vcd()
{
	struct Process *proc;

	if (choosing_flag == FALSE)
	{
		proc = spawn( OpenVCD, 	"Load VCD" );
		if (!proc) PrintMsg(&errmsg,REQTYPE_INFO,REQIMAGE_ERROR);
	}
}

void cmd_open_network()
{
	struct Process *proc;
	if (choosing_flag == FALSE)
	{
		proc = spawn( OpenNetwork, "Open URL" );
		if (!proc) PrintMsg(&errmsg,REQTYPE_INFO,REQIMAGE_ERROR);
	}
}

void cmd_play()
{
	put_command0(MP_CMD_PAUSE);
}

void cmd_stop()
{
	put_command0(MP_CMD_PAUSE);
}

BOOL loop_on()
{
/*
        if (mpctx->loop_times < 0)
		return FALSE;
        else if (mpctx->loop_times == 0)
		return TRUE;
*/
}

void cmd_loop()
{
	if (loop_on()==FALSE)
	{
		put_icommand1(MP_CMD_LOOP,1);
	}
	else
	{
		put_icommand1(MP_CMD_LOOP,-1);
	}
}

void menu_mplayer(ULONG menucode)
{

	printf("menu num %d\n", ITEMNUM(menucode));

	switch (ITEMNUM(menucode))
	{
		case ID_load_file: 
			cmd_load_file(); 
			break;
		case ID_open_dvd:  
			cmd_open_dvd();
			break;
		case ID_open_dvdnav:  
			cmd_open_dvdnav();
			break;
		case ID_open_vcd:
			cmd_open_vcd();
			break;
		case ID_open_network:
			cmd_open_network();
			break;
		case ID_about:
			ShowAbout();
			break;
		case ID_quit: 
			put_command0(MP_CMD_QUIT);
			break;
	}
}

void menu_play(ULONG menucode)
{
	switch (ITEMNUM(menucode))
	{
		case ID_play:
			cmd_play();
			break;
/*
		case ID_stop:
			cmd_stop();
			break;
*/
		case ID_loop:  
			cmd_loop();
			break;

		case ID_prev:
			prevfilm();
			break;

		case ID_next:
			nextfilm();
			break;
	}
}

enum
{
	ID_ontop,
	ID_subtitles,
	ID_fullscreen,
	ID_screenshot
};


void menu_video(ULONG menucode)
{

	switch (ITEMNUM(menucode))
	{
		case ID_ontop:
			put_command0(MP_CMD_VO_ONTOP);
			break;
		case ID_subtitles:
			put_command0(MP_CMD_SUB_SELECT);
			break;
		case ID_fullscreen:
			put_command0(MP_CMD_VO_FULLSCREEN);
			break;
		case ID_screenshot:

			amigaos_screenshot();

//			put_command0(MP_CMD_SCREENSHOT);

			break;
/*
		case 5:
			p96pip_ChangeWindowSize(x1);
			break;
		case 6:
			p96pip_ChangeWindowSize(x2);
			break;
		case 7:
			p96pip_ChangeWindowSize(xF);
			break;
		case 8:
			p96pip_ChangeWindowSize(xH);
			break;
		case 10:
			switch (SUBNUM(menucode))
			{
				case 0:
					AspectRatio = AR_ORIGINAL;
					p96pip_ChangeWindowSize(AR_ORIGINAL);
					break;
				case 2:
					AspectRatio = AR_16_10;
					p96pip_ChangeWindowSize(AR_16_10);
				case 3:
					AspectRatio = AR_16_9;
					p96pip_ChangeWindowSize(AR_16_9);
					break;
				case 4:
					AspectRatio = AR_185_1;
					p96pip_ChangeWindowSize(AR_185_1);
					break;
				case 5:
					AspectRatio = AR_221_1;
					p96pip_ChangeWindowSize(AR_221_1);
					break;
				case 6:
					AspectRatio = AR_235_1;
					p96pip_ChangeWindowSize(AR_235_1);
					break;
				case 7:
					AspectRatio = AR_239_1;
					p96pip_ChangeWindowSize(AR_239_1);
					break;
				case 8:
					AspectRatio = AR_5_3;
					p96pip_ChangeWindowSize(AR_5_3);
					break;
				case 9:
					AspectRatio = AR_4_3;
					p96pip_ChangeWindowSize(AR_4_3);
					break;
				case 10:
					AspectRatio = AR_5_4;
					p96pip_ChangeWindowSize(AR_5_4);
					break;
				case 11:
					AspectRatio = AR_1_1;
					p96pip_ChangeWindowSize(AR_1_1);
					break;
			}
*/
	}

}

void menu_audio(ULONG menucode)
{
	switch (ITEMNUM(menucode))
	{
		case ID_mute:
			mp_input_queue_cmd(mp_input_parse_cmd("mute"));
			break;
		case ID_volume_up:
			mplayer_put_key(KEY_VOLUME_UP);
			break;
		case ID_volume_down:
			mplayer_put_key(KEY_VOLUME_DOWN);
			break;
	}
}

void menu_events( struct IntuiMessage *IntuiMsg)
{
	ULONG menucode = IntuiMsg->Code;

	if (menucode != MENUNULL)
	{
		while (menucode != MENUNULL)
		{
			struct MenuItem  *item = ItemAddress(menu,menucode);

			select_menu[ MENUNUM(menucode) ] ( menucode );

			/* Handle multiple selection */
			menucode = item->NextSelect;
		}
	}
}


void ShowAbout(void)
{
	struct Process *TaskMessage;

	if (choosing_flag == TRUE) return;


	struct pmpMessage *pmp = AllocVec(sizeof(struct pmpMessage), MEMF_SHARED);

	if(!pmp)
   	 return;

	pmp->about = "MPlayer - bounty version\n"
				"\nBuilt against MPlayer version: "	VERSION
				"\nAmiga version: " AMIGA_VERSION
				"\n" FFMPEG_VERSION
				"\n\nCopyright (C) MPlayer Team - http://www.mplayerhq.hu"
				"\n\nAmigaOS4 Version:"
				"\n\nAndrea Palmate' - http://www.amigasoft.net"
				"\nand Kjetil Hvalstrand (2014-2015) - http://database-org.com";

	pmp->type = REQTYPE_INFO;
	pmp->image = REQIMAGE_INFO;

	TaskMessage = (struct Process *) CreateNewProcTags (
		   							NP_Entry, 	(ULONG) PrintMsgProc,
						   			NP_Name,   	"About MPlayer",
									NP_StackSize,   262144,
									NP_Child,		TRUE,
									NP_Priority,	0,
					   				NP_EntryData, 	pmp,
					   				NP_ExitData, 	IDOS,
									NP_FinalCode, spawn_died,
						  			TAG_DONE);

	if (TaskMessage) spawn_count++;

	if (!TaskMessage)
	{
		PrintMsg(pmp->about,REQTYPE_INFO,REQIMAGE_INFO);
		FreeVec(pmp);
	}
}

static void OpenNetwork(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED)
{
	Object *netwobj;
	UBYTE buffer[513]="http://";
	ULONG result;

	netwobj = (Object *) NewObject(REQUESTER_GetClass(), NULL,		
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
		choosing(TRUE);
		result = IDoMethod( netwobj, RM_OPENREQ, NULL, NULL, Menu_Window -> WScreen, TAG_END );
		choosing(FALSE);

		if (result != 0 && (strcmp(buffer,"http://")!=0))
		{
			PlayFile_async(buffer);
		}
		DisposeObject( netwobj );

	}
}


static void OpenDVD(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED)
{
	Object *dvdobj;
	UBYTE buffer[256]="dvd://1";
	ULONG result;

	dvdobj = (Object *) NewObject(REQUESTER_GetClass(),NULL,
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
		choosing(TRUE);
		result = IDoMethod( dvdobj, RM_OPENREQ, NULL, NULL, Menu_Window -> WScreen, TAG_END );
		choosing(FALSE);

		if (result != 0 && (strncmp(buffer,"dvd://",6)==0))
		{
   			PlayFile_async(buffer);
			printf("buffer:%s\n",buffer);
		}
		else
		{
			if (strncmp(buffer,"dvd://",6)!=0) 	PrintMsg("Enter a valid DVD protocol", REQTYPE_INFO, REQIMAGE_ERROR);
		}
		DisposeObject( dvdobj );
	}
}

static void OpenDVDNAV(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED)
{
	Object *dvdobj;
	UBYTE buffer[256]="dvdnav://";
	ULONG result;

	if (stream_cache_size>0)
	{
		PrintMsg("To use DVDNAV, you most comment out cache in the config file", REQTYPE_INFO, REQIMAGE_INFO);
		return;
	}

	dvdobj = (Object *) NewObject(REQUESTER_GetClass(),NULL,
							REQ_Type,       REQTYPE_STRING,
							REQ_TitleText,  "Load DVDNAV",
							REQ_BodyText,   "Enter Chapter to Open",
							REQ_GadgetText, "O_pen|_Cancel",
        						REQS_Invisible,   FALSE,
        						REQS_Buffer,      buffer,
							REQS_ShowDefault, TRUE,
							REQS_MaxChars,    255,
							TAG_DONE);
	if ( dvdobj )
	{
	choosing(TRUE);
		result = IDoMethod( dvdobj, RM_OPENREQ, NULL, NULL, Menu_Window -> WScreen, TAG_END );
		choosing(FALSE);

		if (result != 0 && (strncmp(buffer,"dvdnav://",9)==0))
		{
   			PlayFile_async(buffer);
			printf("buffer:%s\n",buffer);
		}
		else
		{
			if (strncmp(buffer,"dvdnav://",9)!=0) PrintMsg("Enter a valid DVD protocol", REQTYPE_INFO, REQIMAGE_ERROR);
		}
		DisposeObject( dvdobj );
	}
}



static void OpenVCD(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED)
{

	Object *vcdobj;
	UBYTE buffer[256]="vcd://1";
	ULONG result;

	vcdobj = (Object *) NewObject(REQUESTER_GetClass(),NULL,
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
		choosing(TRUE);
		result = IDoMethod( vcdobj, RM_OPENREQ, NULL, NULL, Menu_Window -> WScreen, TAG_END );
		choosing(FALSE);

		if (result != 0 && (strncmp(buffer,"vcd://",6)==0))
		{
			PlayFile_async(buffer);
		}
		else
			if (strncmp(buffer,"vcd://",6)!=0) PrintMsg("Enter a valid VCD protocol", REQTYPE_INFO, REQIMAGE_ERROR);
		DisposeObject( vcdobj );
	}

}

static UBYTE *LoadFile(const char *StartingDir)
{
	UBYTE *filename = NULL;

	struct FileRequester * AmigaOS_FileRequester = NULL;
	BPTR FavoritePath_File;
	char FavoritePath_Value[1024];
	BOOL FavoritePath_Ok = FALSE;

	int len = 0;

	if (StartingDir==NULL)
	{
		FavoritePath_File = Open("PROGDIR:FavoritePath", MODE_OLDFILE);
		if (FavoritePath_File)
		{
			LONG size = Read(FavoritePath_File, FavoritePath_Value, 1024);
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
	}
	else
	{
		FavoritePath_Ok = TRUE;
		strcpy(FavoritePath_Value, StartingDir);
	}

	AmigaOS_FileRequester = AllocAslRequest(ASL_FileRequest, NULL);
	if (!AmigaOS_FileRequester)
	{
		mp_msg(MSGT_CPLAYER, MSGL_FATAL,"Cannot open the FileRequester!\n");
		return NULL;
	}

	choosing(TRUE);
	if ( ( AslRequestTags( AmigaOS_FileRequester,
					ASLFR_Screen, Menu_Window -> WScreen,
					ASLFR_TitleText,        "Choose a video",
					ASLFR_DoMultiSelect,    FALSE, //maybe in a future we can implement a playlist..
					ASLFR_RejectIcons,      TRUE,
					ASLFR_DoPatterns,		TRUE,
					ASLFR_StayOnTop,		TRUE,
					ASLFR_InitialPattern,	(ULONG)EXTPATTERN,
					ASLFR_InitialDrawer,    (FavoritePath_Ok) ? FavoritePath_Value : "",
					TAG_DONE) ) == FALSE )
	{
		FreeAslRequest(AmigaOS_FileRequester);
		AmigaOS_FileRequester = NULL;
		choosing(FALSE);
		return NULL;
	}
	choosing(FALSE);

	if (AmigaOS_FileRequester->fr_NumArgs>0);
	{
		len = strlen(AmigaOS_FileRequester->fr_Drawer) + strlen(AmigaOS_FileRequester->fr_File) + 1;
		if ((filename = AllocMem(len,MEMF_SHARED|MEMF_CLEAR)))
		{
			strcpy(filename,AmigaOS_FileRequester->fr_Drawer);
			AddPart(filename,AmigaOS_FileRequester->fr_File,len + 1);
		}
	}

	FreeAslRequest(AmigaOS_FileRequester);

	printf("%d:%s = '%s'\n",__LINE__,__FUNCTION__,filename);

	return filename;

}

static int32 PrintMsgProc(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase)
{
        struct ExecIFace *IExec = (struct ExecIFace *) execbase->MainInterface;
        struct Process *me = (struct Process *) FindTask(NULL);
        struct DOSIFace *IDOS = (struct DOSIFace *) me->pr_ExitData;
        struct pmpMessage *pmp = (struct pmpMessage *) GetEntryData();

        if(pmp)
        {
                PrintMsg(pmp->about, pmp->type, pmp->image);
                FreeVec(pmp);
                return RETURN_OK;
        }
        else
        	return RETURN_FAIL;

}

void PrintMsg(CONST_STRPTR text, int REQ_TYPE, int REQ_IMAGE)
{
	Object *reqobj;

	if (REQ_TYPE == 0) REQ_TYPE = REQTYPE_INFO;
	if (REQ_IMAGE == 0) REQ_IMAGE = REQIMAGE_DEFAULT;

	reqobj = (Object *) NewObject( REQUESTER_GetClass(), NULL,
		REQ_Type,	REQ_TYPE,
		REQ_TitleText,		"MPlayer for AmigaOS4",
		REQ_BodyText,	text,
		REQ_Image,		REQ_IMAGE,
		REQ_TimeOutSecs,	10,
		REQ_GadgetText,	"Ok",
		TAG_END );

	if (reqobj)
	{

		choosing(TRUE);		// not selecing file, but need to know, if a window is open.

		if (Menu_Window)
		{
			IDoMethod( reqobj, RM_OPENREQ, NULL, NULL, Menu_Window -> WScreen, TAG_END );
		}
		else
		{
			IDoMethod( reqobj, RM_OPENREQ, NULL, NULL, NULL, TAG_END );
		}

		choosing(FALSE);

		DisposeObject( reqobj );
	}
}

void add_file_to_que( const char *FileName)
{
	mp_cmd_t MPCmd;
	mp_cmd_t *c;

	printf("MP_CMD_LOADFILE = %d\n",MP_CMD_LOADFILE);

	bzero(&MPCmd,sizeof(MPCmd));

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
	{
		if ((mp_input_queue_cmd(c)==0))
		{
			mp_cmd_free(c);
		}
	}
	else
	{
		printf("failed to execute\n");
	}
}



void lastsong()
{
	mp_cmd_t MPCmd;
	mp_cmd_t *c;

	printf("MP_CMD_PLAY_TREE_UP_STEP = %d\n",MP_CMD_PLAY_TREE_UP_STEP);

	bzero(&MPCmd,sizeof(MPCmd));

	MPCmd.id			= MP_CMD_PLAY_TREE_UP_STEP; 
	MPCmd.name			= NULL;
	MPCmd.nargs			= 2;
	MPCmd.pausing		= FALSE;
	MPCmd.args[0].type	= MP_CMD_ARG_INT;
	MPCmd.args[0].v.i	= 1;
	MPCmd.args[1].type	= MP_CMD_ARG_INT;
	MPCmd.args[1].v.i	= TRUE;
	MPCmd.args[2]		= mp_cmds[MP_CMD_PLAY_TREE_STEP].args[2];

	if ((c = mp_cmd_clone(&MPCmd)))
	{
		if ((mp_input_queue_cmd(c)==0))
		{
			mp_cmd_free(c);
		}
		else
		{
			printf("failed to que\n");
		}
	}
	else
	{
		printf("failed to execute\n");
	}
}

void prevfilm()
{
	mp_cmd_t MPCmd;
	mp_cmd_t *c;

	printf("MP_CMD_PLAY_TREE_STEP = %d\n",MP_CMD_PLAY_TREE_STEP);

	bzero(&MPCmd,sizeof(MPCmd));

	MPCmd.id			= MP_CMD_PLAY_TREE_STEP; 
	MPCmd.name			= NULL;
	MPCmd.nargs			= 2;
	MPCmd.pausing		= FALSE;
	MPCmd.args[0].type	= MP_CMD_ARG_INT;
	MPCmd.args[0].v.i	= -1;
	MPCmd.args[1].type	= MP_CMD_ARG_INT;
	MPCmd.args[1].v.i	= TRUE;
	MPCmd.args[2]		= mp_cmds[MP_CMD_PLAY_TREE_STEP].args[2];

	if ((c = mp_cmd_clone(&MPCmd)))
	{
		if ((mp_input_queue_cmd(c)==0))
		{
			mp_cmd_free(c);
		}
	}
	else
	{
		printf("failed to execute\n");
	}
}

void nextfilm()
{
	mp_cmd_t MPCmd;
	mp_cmd_t *c;

	printf("MP_CMD_PLAY_TREE_STEP = %d\n",MP_CMD_PLAY_TREE_STEP);

	bzero(&MPCmd,sizeof(MPCmd));

	MPCmd.id			= MP_CMD_PLAY_TREE_STEP; 
	MPCmd.name			= NULL;
	MPCmd.nargs			= 2;
	MPCmd.pausing		= FALSE;
	MPCmd.args[0].type	= MP_CMD_ARG_INT;
	MPCmd.args[0].v.i	= 1;
	MPCmd.args[1].type	= MP_CMD_ARG_INT;
	MPCmd.args[1].v.i	= TRUE;
	MPCmd.args[2]		= mp_cmds[MP_CMD_PLAY_TREE_STEP].args[2];

	if ((c = mp_cmd_clone(&MPCmd)))
	{
		if ((mp_input_queue_cmd(c)==0))
		{
			mp_cmd_free(c);
		}
	}
	else
	{
		printf("failed to execute\n");
	}
}


void seek_start()
{
	mp_cmd_t MPCmd;
	mp_cmd_t *c;

	printf("MP_CMD_SEEK = %d\n",MP_CMD_SEEK);

	bzero(&MPCmd,sizeof(MPCmd));

	MPCmd.pausing = FALSE;
	MPCmd.id    = MP_CMD_SEEK;
	MPCmd.name  = strdup("seek");
	MPCmd.nargs = 2;
	MPCmd.args[0].type = MP_CMD_ARG_FLOAT;
	MPCmd.args[0].v.f=0/1000.0f;
	MPCmd.args[1].type = MP_CMD_ARG_INT;
	MPCmd.args[1].v.i=2;
	MPCmd.args[2].type = -1;
	MPCmd.args[2].v.i=0;
	if (( c = mp_cmd_clone(&MPCmd)))
	{
		if ((mp_input_queue_cmd(c)==0))
		{
			mp_cmd_free(c);
		}
		else
		{
			printf("failed to que\n");
		}
	}
	else
	{
		printf("failed to clone\n");
	}
}

