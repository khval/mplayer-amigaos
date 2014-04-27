#include "playtree.h"
#include "m_config.h"
#include "gui/interface.h"

#define mp_basename(s) (strrchr(s,'/')==NULL?(char*)s:(strrchr(s,'/')+1))

static void guiSetEvent(int event);

struct gui_t
{
	ULONG colorkey; /* overlay colorkey pen */
	void (*playercontrol)(int event);   /* userdefined call back function (guiSetEvent) */

	ULONG embedded;
};

int guiWinID=0;
char *skinName="default";
guiInterface_t guiIntfStruct;
struct gui_t *mygui = NULL;

void initguimembers(void)
{
	const vo_functions_t * vo;

	memset(mygui, 0, sizeof(*mygui));

	// Shall embedded mode be used ?
	mygui->embedded = TRUE;

	mygui->colorkey = 0;
	mygui->playercontrol = guiSetEvent;
}

/* GUI functions */
int cfg_read( void )
{
	return 1;
}


void guiInit( void )
{
	p96_OpenOSLibs();
	
	memset( &guiIntfStruct,0,sizeof( guiIntfStruct ) );
	
	guiIntfStruct.Balance	= 50.0f;
	guiIntfStruct.StreamType= -1;
	guiIntfStruct.Playing	= 0;	

	config(640, 480, 640, 480, 0, (char *)"MPlayer for AmigaOS4", 0);	
	//Open_PIPWindow();
}

void guiEventHandling( void )
{
	check_events();
}

static void guiSetEvent(int event)
{
}

int guiGetEvent(int type, void *arg)
{
    switch (type)
    {
		case guiCEvent:
			switch ( (int)arg )
			{
				case guiSetPlay:
                    printf("guiSetPlay\n");
					guiIntfStruct.Playing=1;
				break;
				case guiSetStop:
                    printf("guiSetStop\n");
					guiIntfStruct.Playing=0;
				break;
				case guiSetPause: 
                    printf("guiSetPause\n");
					guiIntfStruct.Playing=2; 
				break;
			}
			mplState();
		break;

        case guiIEvent:
        {
            switch((int) arg)
            {
                case MP_CMD_VO_FULLSCREEN:
                    printf("GoFullscreen");
                    break;
                case MP_CMD_QUIT:
                {
					guiDone();
                    exit_player("Done");
                    return 0;
                }
                case MP_CMD_STOP:
                    printf("STOP");
                    break;
                case MP_CMD_PAUSE:
                    printf("PAUSE");
                    break;
            }
        }
		break;
		case guiSetShVideo:
			printf("guiSetShVideo\n");
		break;
		break;
		case 10:
		break;
		default:
			printf("guiGetEvent:%d\n",type);
        break;
	}
	
	return 1;
}

void guiDone( void )
{
	FreeGfx();
	p96_CloseOSLibs();
}

void mplEnd( void )
{
	printf("mplEnd\n");
}

void mplState( void )
{
	if ( ( guiIntfStruct.Playing == 0 )||( guiIntfStruct.Playing == 2 ) )
	{
		printf("disable play button\n");
	}
	else
	{
		printf("enable play button\n");
	}
}

void mplNext(void)
{
    if(guiIntfStruct.Playing == 2) return;
#if 0
    switch(guiIntfStruct.StreamType)
    {
#ifdef CONFIG_DVDREAD
        case STREAMTYPE_DVD:
            if(guiIntfStruct.DVD.current_chapter == (guiIntfStruct.DVD.chapters - 1))
                return;
            guiIntfStruct.DVD.current_chapter++;
            break;
#endif
        default:
            if(mygui->playlist->current == (mygui->playlist->trackcount - 1))
                return;
            mplSetFileName(NULL, mygui->playlist->tracks[(mygui->playlist->current)++]->filename,
                           STREAMTYPE_STREAM);
            break;
    }
    mygui->startplay(mygui);
#endif    
}

void mplPrev(void)
{
    if(guiIntfStruct.Playing == 2) return;
#if 0
    switch(guiIntfStruct.StreamType)
    {
#ifdef CONFIG_DVDREAD
        case STREAMTYPE_DVD:
            if(guiIntfStruct.DVD.current_chapter == 1)
                return;
            guiIntfStruct.DVD.current_chapter--;
            break;
#endif
        default:
            if(mygui->playlist->current == 0)
                return;
            mplSetFileName(NULL, mygui->playlist->tracks[(mygui->playlist->current)--]->filename,
                           STREAMTYPE_STREAM);
            break;
    }
    mygui->startplay(mygui);
#endif
}


int import_initial_playtree_into_gui(play_tree_t* my_playtree, m_config_t* config, int enqueue)
{
  int result=0;
printf("import_initial_playtree_into_gui\n");
#if 0
  play_tree_iter_t* my_pt_iter=NULL;

  if (!enqueue) // Delete playlist before "appending"
    gtkSet(gtkDelPl,0,0);

  if((my_pt_iter=pt_iter_create(&my_playtree,config)))
  {
    while ((filename=pt_iter_get_next_file(my_pt_iter))!=NULL)
    {
      if (import_file_into_gui(filename, 0)) // Add it to end of list
        result=1;
    }
  }

  mplCurr(); // Update filename
  mplGotoTheNext=1;

  filename=NULL;
#endif

  return result;
}

int import_file_into_gui(char* temp, int insert)
{
printf("import_file_into_gui\n");

	char *filename, *pathname;
	plItem * item;

	filename = strdup(mp_basename(temp));
	pathname = strdup(temp);
	if (strlen(pathname)-strlen(filename)>0)
    	pathname[strlen(pathname)-strlen(filename)-1]='\0'; // We have some path so remove / at end
  	else
    	pathname[strlen(pathname)-strlen(filename)]='\0';
  	mp_msg(MSGT_PLAYTREE,MSGL_V, "Adding filename %s && pathname %s\n",filename,pathname); //FIXME: Change to MSGL_DBG2 ?
  	item=calloc( 1,sizeof( plItem ) );
  	if (!item)
		return 0;
	item->name=filename;
	item->path=pathname;
#if 0
	if (insert)
    	gtkSet( gtkInsertPlItem,0,(void*)item ); // Inserts the item after current, and makes current=item
	else
		gtkSet( gtkAddPlItem,0,(void*)item );
#endif
	return 1;
}

int import_playtree_playlist_into_gui(play_tree_t* my_playtree, m_config_t* config)
{
printf("import_playtree_playlist_into_gui\n");
  play_tree_iter_t* my_pt_iter=NULL;
  int result=0;
  if((my_pt_iter=pt_iter_create(&my_playtree,config)))
  {
    while ((filename=pt_iter_get_next_file(my_pt_iter))!=NULL)
    {
      if (import_file_into_gui(filename, 1)) // insert it into the list and set plCurrent=new item
        result=1;
    }
    pt_iter_destroy(&my_pt_iter);
  }

  filename=NULL;

  return result;
}
