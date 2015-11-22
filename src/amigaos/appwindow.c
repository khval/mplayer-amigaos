/* This code comes from Excalibur */
/* Written by: Kjetil Hvalstrand */
/* Licence: MIT  (take use, modify, give creadit) */


#include <proto/Exec.h>
#include <proto/DOS.h>
#include <proto/intuition.h>
#include <proto/wb.h>
#include <workbench/startup.h>
//include <workbench/workbench.h>

#include <stdlib.h>
#include <string.h>

static struct MsgPort 	*appwin_port = NULL;
static struct AppWindow	*app_window = NULL;
struct AppMessage *appmsg = NULL;

extern int32 PlayFile_async(const char *FileName);

ULONG appwindow_sig = 0;

void make_appwindow(struct Window *win );

void make_appwindow(struct Window *win )
{
	if (!appwin_port) appwin_port = AllocSysObject( ASOT_PORT, TAG_END);

	if (appwin_port)
	{
		appwindow_sig = 1L<< appwin_port -> mp_SigBit;
		app_window = AddAppWindowA(0,0, win,appwin_port,NULL);
	}
}

void delete_appwindow()
{
	if (app_window)
	{
		RemoveAppWindow(app_window) ;
		app_window = NULL;
	}

	if (appwin_port)
	{
		FreeSysObject(ASOT_PORT, (APTR) appwin_port);
		appwin_port = NULL;
		appwindow_sig = 0;
	}
}

char *to_name_and_path(char *path, char *name)
{
	char *path_whit_name = NULL;

	if (path_whit_name = (void *) malloc ( 	strlen(path) + strlen(name) + 2) )
	{
		path_whit_name[0] = 0;

		if (strlen(path)>0)
		{
			if (path[strlen(path)-1]==':')
			{
				sprintf(path_whit_name,"%s%s",path,name);				
			}
			else
			{
				sprintf(path_whit_name,"%s/%s",path,name);
			}
		}
	}

	return path_whit_name;
}


void do_appwindow()
{
	char	temp[1000];
	char	*path_whit_name;
	BPTR temp_lock;
	int n;

	if (appwin_port)
	{
		if (appmsg = (void *) GetMsg(appwin_port))
		{
			for (n=0;n<appmsg -> am_NumArgs;n++)
			{
				if (temp_lock = DupLock(appmsg -> am_ArgList[n].wa_Lock))
				{
					if (NameFromLock(temp_lock,temp,1000))
					{
						if (appmsg -> am_ArgList[n].wa_Name[0]!=0)
						{

							if (path_whit_name = to_name_and_path( temp, appmsg->am_ArgList[n].wa_Name ))
							{
								PlayFile_async(path_whit_name);
								free(path_whit_name);
							}
						}
					}
						
					UnLock(temp_lock);
				}
			}
			ReplyMsg( (void *) appmsg);
		}
	}
}

