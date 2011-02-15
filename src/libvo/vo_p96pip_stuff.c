/*
 *  vo_p96_pip.c
 *  VO module for MPlayer AmigaOS4
 *  using p96 PIP
 *  Written by Joerg Strohmayer and Andrea Palmate'
*/
#undef __USE_INLINE__

void ChangePauseButton(void)
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

static int32 PlayFile(const char *FileName)
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
	ChangePauseButton();

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
	ChangePauseButton();

	return RETURN_OK;
}

static void NotifyServer(STRPTR FileName)
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

void RemoveAppPort(void)
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

static void appw_events (void)
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
					NotifyServer(cmd);
					PlayFile(cmd);
				}
				else
				{
					char *file;
					file = LoadFile(cmd);
					if (file)
					{
						NotifyServer(file);
						PlayFile(file);
					}
				}
		    	IExec->ReplyMsg ((void*) msg);
			}
		}
    }
}

/* STUFF FUNCTIONS FOR MENU */
static void CalcAspectRatio(float a_ratio, int _oldwidth, int _oldheight, int *_newwidth, int *_newheight)
{
	int tempLen = 0;

	tempLen = _oldwidth / a_ratio;
	*_newheight = tempLen;
	*_newwidth = _oldwidth;
}

static void ChangeWindowSize(int mode)
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
				CalcAspectRatio(1.333333f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_5_3:
				CalcAspectRatio(1.666667f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_5_4:
				CalcAspectRatio(1.25f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_16_10:
				CalcAspectRatio(1.6f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_16_9:
				CalcAspectRatio(1.777777f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_235_1:
				CalcAspectRatio(2.35f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_239_1:
				CalcAspectRatio(2.39f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_221_1:
				CalcAspectRatio(2.21f, keep_width, keep_height, &new_width, &new_height);
			break;
			case AR_185_1:
				CalcAspectRatio(1.85f, keep_width, keep_height, &new_width, &new_height);
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

static void SetVolume(int direction)
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

static UBYTE *LoadFile(const char *StartingDir)
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

static int32 PrintMsgProc(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase)
{
        struct ExecIFace *iexec = (APTR)execbase->MainInterface;
        struct Process *me = (APTR)iexec->FindTask(NULL);
        struct DOSIFace *idos = (APTR)me->pr_ExitData;
        struct pmpMessage *pmp = (struct pmpMessage *)idos->GetEntryData();

        if(pmp)
        {
                PrintMsg(pmp->about, pmp->type, pmp->image);
                iexec->FreeVec(pmp);
                return RETURN_OK;
        }
        else
        	return RETURN_FAIL;

}

static void PrintMsg(CONST_STRPTR text,int REQ_TYPE, int REQ_IMAGE)
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

static void ShowAbout(void)
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
		   										NP_Entry, 		(ULONG) PrintMsgProc,
									   			NP_Name,	   	"About MPlayer",
												NP_StackSize,   262144,
												NP_Child,		TRUE,
												NP_Priority,	0,
					   							NP_EntryData, 	pmp,
					   							NP_ExitData, 	IDOS,
									  			TAG_DONE);
	if (!TaskMessage)
	{
		PrintMsg(pmp->about,REQTYPE_INFO,REQIMAGE_INFO);
		IExec->FreeVec(pmp);
	}
}

static void OpenNetwork(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED)
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
			PlayFile(buffer);
		}
		IIntuition->DisposeObject( netwobj );
		choosing = 0;
	}
}

static void OpenDVD(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED)
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
			PlayFile(buffer);
			printf("buffer:%s\n",buffer);
		}
		else
			if (strncmp(buffer,"dvd://",6)!=0)
				PrintMsg("Enter a valid DVD protocol", REQTYPE_INFO, REQIMAGE_ERROR);
		IIntuition->DisposeObject( dvdobj );
		choosing = 0;
	}
}

static void OpenVCD(STRPTR args UNUSED, int32 length UNUSED, struct ExecBase *execbase UNUSED)
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
			PlayFile(buffer);
		}
		else
			if (strncmp(buffer,"vcd://",6)!=0)
				PrintMsg("Enter a valid VCD protocol", REQTYPE_INFO, REQIMAGE_ERROR);
		IIntuition->DisposeObject( vcdobj );
		choosing = 0;
	}
}

void TimerReset(uint32 microDelay)
{
	if (microDelay>0)
	{
		_timerio->Request.io_Command = TR_ADDREQUEST;
		_timerio->Time.Seconds = 0;
		_timerio->Time.Microseconds = microDelay;
		IExec->SendIO((struct IORequest *)_timerio);
	}
}

BOOL TimerInit(void)
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

void TimerExit(void)
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
