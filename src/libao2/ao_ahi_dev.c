/*
 *  ao_ahi_dev.c
 *  libao2 module for MPlayer MorphOS
 *  using AHI/device API
 *  Written by DET Nicolas
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osdep/timer.h>

#include "mp_msg.h"

// NOTE: all OS Specific stuff have not been protected by #ifndef ... #else .. #endif
// because il you compile ahi, you are using on MorphOS (or AmigaOS)

// Some OS Specific include



#ifdef __morphos__
#include <proto/alib.h>
#endif

#include <proto/ahi.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <dos/dostags.h>
#include <dos/dos.h>

#ifdef __morphos__
#include <hardware/byteswap.h>
#endif


struct Library 			 *AHIBase = NULL;
struct AHIIFace		*IAHI = NULL;

#ifdef __amigaos4__

#define SWAPLONG( i, o ) asm ( "lwarx %0,0,%1 \n"  :  "=r" (o) : "r" (&i), "r" (&i)  : "r0" );
#define SWAPWORD( hi, ho ) asm ( "lhbrx %0,0,%1 \n"	:  "=r" (ho) : "r" (&hi), "r" (&hi)  : "r0" );

#endif

// End OS Specific

#include "../libaf/af_format.h"
#include "audio_out.h"
#include "audio_out_internal.h"

// DEBUG

#define debug_level 0

#if debug_level > 0
BPTR output;
#define kk(x) x
#else
#define kk(x)
#endif
#if debug_level > 1
#define KPrintF(...) Printf( __VA_ARGS__ )
#else
#define KPrintF(...) 
#endif

//some define for this nice AHI driver !
#define AHI_CHUNKSIZE         (65536/2)
#define AHI_CHUNKMAX          (131072/2)
#define AHI_DEFAULTUNIT       0


/****************************************************************************/

BOOL AHIDevice;

static struct MsgPort *     AHIMsgPort  = NULL;     // The msg port we will use for the ahi.device
static struct AHIRequest *  AHIio       = NULL;     // First AHIRequest structure
static struct AHIRequest *  AHIio2      = NULL;     // second one. Double buffering !
static struct AHIRequest *  AHIio_orig       = NULL;     // First AHIRequest structure
static struct AHIRequest *  AHIio2_orig      = NULL;     // second one. Double buffering !
static struct AHIRequest *link=NULL;


static ULONG AHIType;
static LONG AHI_Volume=0x10000;

// some AmigaOS Signals
static BYTE sig_taskready=-1;
static BYTE sig_taskend=-1;
 
static struct Process *PlayerTask_Process = NULL;
static struct Task    *MainTask = NULL;           // Pointer to the main task;

#define PLAYERTASK_NAME       "MPlayer Audio Thread"
#define PLAYERTASK_PRIORITY   2

static float audio_bps = 1.0f;
//struct timeval tv_read, tv_write;

unsigned int tv_write =0;

//static struct timezone dontcare = { 0,0 };


// Buffer management
static struct SignalSemaphore LockSemaphore;
static BOOL buffer_full=FALSE;     // Indicate if the buffer is full or not
static ULONG buffer_get=0; // place where we have to get the buffer to play
static ULONG buffer_put=0; // Place where we have to put the buffer we will play


static unsigned int written_byte=0;
static BYTE *buffer=NULL;    // Our audio buffer
static paused = FALSE;

typedef enum
{
  CO_NONE,
  CO_8_U2S,
  CO_16_U2S,
  CO_16_LE2BE,
  CO_16_LE2BE_U2S,
  CO_24_BE2BE,
  CO_24_LE2BE,
  CO_32_LE2BE
} convop_t;
convop_t convop;

//Custom function internal to this driver
inline static void cleanup(void); // Clean !
static void PlayerTask (void);

static ao_info_t info = 
{
#ifdef __MORPHOS__
   "AHI audio output using high-level API (MorphOS)",
#else
   "AHI audio output using high-level API (AmigaOS)",
#endif
   "ahi_dev",
   "DET Nicolas",
#ifdef __MORPHOS__
   "MorphOS Rulez :-)"
#else
   "Amiga Survivor !",
#endif
};

// define the standart functions that mplayer will use.
// see define in audio_aout_internal.h
// LIBAO_EXTERN(ahi_dev)

static int ahi_dev_init(int rate,int channels,int format,int flags);
static void ahi_dev_uninit(int immed);

const ao_functions_t audio_out_ahi_dev =
{
	&info,
	control,
	ahi_dev_init,
        ahi_dev_uninit,
	reset,
	get_space,
	play,
	get_delay,
	audio_pause,
	audio_resume
};


/*******************************************************************/
static void cleanup_task(void) {

   kk(KPrintF("Closing device\n");)

	if ( IAHI ) DropInterface((struct Interface*)IAHI); IAHI = 0;

	if (! AHIDevice )
	{
		if (link)
		{
			AbortIO( (struct IORequest *) link);
			WaitIO( (struct IORequest *) link);
		}

		CloseDevice ( (struct IORequest *) AHIio_orig);
	}



   kk(KPrintF("Deleteting AHIio_orig\n");)
   if (AHIio_orig) DeleteIORequest( (struct IORequest *)AHIio_orig);

   kk(KPrintF("Freeing AHIio2_orig\n");)
   if (AHIio2_orig) FreeMem( AHIio2_orig,  sizeof(struct AHIRequest) );


   kk(KPrintF("Deleting AHIMsgPort\n");)
   if (AHIMsgPort) DeleteMsgPort(AHIMsgPort);


   kk(KPrintF("Freeing Buffer\n");)
   if (buffer) FreeVec(buffer );

   kk(KPrintF("The end\n");)
   link = NULL;
   buffer = NULL;
   AHIMsgPort = NULL;
   AHIio = NULL;
   AHIio_orig = NULL;
   AHIio2 = NULL;
   AHIio2_orig = NULL;
   AHIDevice = -1;       // -1 -> Not opened !
}

/*******************************************************************/
static void cleanup(void) {
	if (buffer) 
	{
		FreeVec(buffer);
		buffer = NULL;
	}
	if (sig_taskready != -1)
	{
		FreeSignal(sig_taskready);
		sig_taskready = -1;
	}

	if (sig_taskend != -1)
	{
		FreeSignal(sig_taskend);
		sig_taskend = -1;
	}

	buffer_get=0;
	buffer_put=0;
	written_byte=0;
}

/******************** PLAYER TASK ********************/
static void PlayerTask (void) {
int buffer_to_play_len;

// for AHI
struct AHIRequest *tempRequest;

struct timeval after;
unsigned int microsec;


// Code starts here
kk(KPrintF("TASK: *********************************************\n");)
kk(KPrintF("TASK: PlayerTask starts\n");)


kk(KPrintF("TASK: sig_taskready: %d   sig_taskready %d\n", sig_taskready, sig_taskend );)

// Init AHI

   if( ( AHIMsgPort=CreateMsgPort() )) {
	  if ( (AHIio=(struct AHIRequest *)CreateIORequest(AHIMsgPort,sizeof(struct AHIRequest)))) {
		 AHIio->ahir_Version = 4;
		 AHIDevice = OpenDevice(AHINAME, AHI_DEFAULTUNIT,(struct IORequest *) AHIio, 0);

#ifdef __amigaos4__
		AHIBase = (struct Library *)AHIio->ahir_Std.io_Device;
		IAHI = (struct AHIIFace*) GetInterface(AHIBase,"main",1L,NULL) ;
#endif

	  }
   }


   if(AHIDevice) {
	  kk(KPrintF("Unable to open %s/0 version 4\n",AHINAME);)
	  goto end_playertask;
   }


   // Make a copy of the request (for double buffering)

   AHIio2 = AllocMem(sizeof(struct AHIRequest), MEMF_ANY);
   if(! AHIio2) {
	  goto end_playertask;
   }
  
   // Backup original pointer 
   AHIio_orig = AHIio;
   AHIio2_orig= AHIio2;

   memcpy(AHIio2, AHIio, sizeof(struct AHIRequest));

   kk(KPrintF("TASK: Sends task ready signal to MainTask\n");)
   Signal( (struct Task *) MainTask, 1L << sig_taskready );

	kk(KPrintF("TASK: Starting main loop\n");)

	for(;;)
	{
		if ( (SetSignal(0, SIGBREAKF_CTRL_C ) & SIGBREAKF_CTRL_C ) == SIGBREAKF_CTRL_C ) break;

		if (paused)
		{
			Delay(TICKS_PER_SECOND / 10);
			continue;
		}

		ObtainSemaphore(&LockSemaphore); 
		kk(KPrintF("TASK: Semaphore obtained\n");)

		buffer_to_play_len =  buffer_put - buffer_get;
		if ( (!buffer_to_play_len) && (!buffer_full)  )
		{
			kk(KPrintF("TASK: BufferEmpty\n");) 
			ReleaseSemaphore(&LockSemaphore); 
			kk(KPrintF("TASK: Semaphore released\n");)
			Delay(1);
			continue; 
		} // buffer empty -> do nothing
   

		if ( buffer_to_play_len > 0)
		{
			kk(KPrintF("TASK: buffer_get: %d buffer_to_play_len: %d\n", buffer_get, buffer_to_play_len);)
			//if (buffer_to_play_len > AHI_CHUNKSIZE) buffer_to_play_len = AHI_CHUNKSIZE;

			AHIio->ahir_Std.io_Message.mn_Node.ln_Pri = 0;
			AHIio->ahir_Std.io_Command  = CMD_WRITE;
			AHIio->ahir_Std.io_Offset   = 0;
			AHIio->ahir_Frequency       = (ULONG) ao_data.samplerate;
			AHIio->ahir_Volume          = AHI_Volume;       
			AHIio->ahir_Position        = 0x8000;           // Centered
			AHIio->ahir_Std.io_Data     = buffer + buffer_get; 
			AHIio->ahir_Std.io_Length   = (ULONG) buffer_to_play_len; 
			AHIio->ahir_Type = AHIType;
			AHIio->ahir_Link            = link;     
		}
		else
		{
			kk(KPrintF("TASK: buffer_get: %d buffer_to_play_len: %d\n", buffer_get, AHI_CHUNKMAX - buffer_get);)
			buffer_to_play_len = AHI_CHUNKMAX - buffer_get;
			if (buffer_to_play_len > AHI_CHUNKSIZE) buffer_to_play_len = AHI_CHUNKSIZE;
	
			AHIio->ahir_Std.io_Message.mn_Node.ln_Pri = 0;
			AHIio->ahir_Std.io_Command  = CMD_WRITE;
			AHIio->ahir_Std.io_Offset   = 0;
			AHIio->ahir_Frequency       = (ULONG) ao_data.samplerate;
			AHIio->ahir_Volume          = AHI_Volume;       
			AHIio->ahir_Position        = 0x8000;           // Centered
			AHIio->ahir_Std.io_Data     = buffer + buffer_get; 
			AHIio->ahir_Std.io_Length   = (ULONG) buffer_to_play_len;
			AHIio->ahir_Link            = link;
			AHIio->ahir_Type = AHIType;
		}

		buffer_get += buffer_to_play_len;
		buffer_get %= AHI_CHUNKMAX;

		kk(KPrintF("TASK: Semaphore released (next line)\n");)

		ReleaseSemaphore(&LockSemaphore);

		SendIO( (struct IORequest *) AHIio);
		buffer_full=FALSE;
		written_byte -= buffer_to_play_len;    

		if (link)
		{
			WaitIO ( (struct IORequest *) link);

			if (CheckIO((struct IORequest *)AHIio))
			{
				// Playback caught up with us, rebuffer... 
				WaitIO((struct IORequest *)AHIio);
				link = NULL;
				continue;
			}
		}

		link = AHIio;
		// Swap requests
		tempRequest = AHIio;
		AHIio  = AHIio2;
		AHIio2 = tempRequest;

	} 

end_playertask:

	kk(KPrintF("Starting Cleanup\n");)
	cleanup_task();
	kk(KPrintF("Cleanup OK\n");)
	Signal( (struct Task *) MainTask, SIGF_CHILD );
}



/***************************** CONTROL *******************************/
// to set/get/query special features/parameters
static int control(int cmd, void *arg)
{
	switch (cmd)
	{
		case AOCONTROL_GET_VOLUME:
		{
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			vol->left = vol->right =  ( (float) (AHI_Volume >> 8 ) ) / 2.56 ;
			return CONTROL_OK;
		}

		case AOCONTROL_SET_VOLUME:
		{
			float diff;
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			diff = (vol->left+vol->right) / 2;
			AHI_Volume =  ( (int) (diff * 2.56) ) << 8 ;
			return CONTROL_OK;
		}
	}
	return CONTROL_UNKNOWN;
}

/***************************** INIT **********************************/
// open & setup audio device
// return: 1=success 0=fail
static int ahi_dev_init(int rate,int channels,int format,int flags){
	  // Init ao.data to make mplayer happy :-)  
   ao_data.channels=      channels;
   ao_data.samplerate=    rate;
   ao_data.format=        format;
   ao_data.bps=           channels*rate;

	int bits[]={8,16,24,32,40,48};


	   // Check if we are able to do that !

//	format &= (AF_FORMAT_BITS_MASK |  AF_FORMAT_SIGN_MASK | AF_FORMAT_POINT_MASK | AF_FORMAT_END_MASK) ;
//	format ^=  AF_FORMAT_SIGN_MASK;	// swap sign.

	switch ( format ) 
	{
	  case AF_FORMAT_U8:
		ao_data.format =AF_FORMAT_S8;	
		convop  = CO_NONE;
		 AHIType = channels > 1 ? AHIST_S8S : AHIST_M8S;
		break;
	  case AF_FORMAT_S8:
		 convop  = CO_NONE;
		 AHIType = channels > 1 ? AHIST_S8S : AHIST_M8S;
		 break;
	  case AF_FORMAT_S16_BE:
		 convop  = CO_NONE;
		 AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		 break;
	  case AF_FORMAT_S16_LE:
		ao_data.format =AF_FORMAT_S16_BE;
		 convop  = CO_NONE;
		 AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		 break;
	  case AF_FORMAT_U16_LE:
		ao_data.format =AF_FORMAT_S16_BE;
		convop  = CO_NONE;
		AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		break;
	  case AF_FORMAT_U16_BE:
		ao_data.format =AF_FORMAT_S16_BE;
		 convop  = CO_NONE;
		 AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		 break;
	  case AF_FORMAT_S32_BE:
		 convop  = CO_NONE;
		 AHIType = channels > 1 ? AHIST_S32S : AHIST_M32S;
		 break;
	  case AF_FORMAT_S32_LE:
		ao_data.format =AF_FORMAT_S32_BE;
		 convop  = CO_NONE;
		 AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		 break;
	  case AF_FORMAT_AC3:
		 convop  = CO_NONE;
		 AHIType = AHIST_S16S;
		 rate = 48000;
		 break;
	  default:

		printf("Not supported audio format %08x\n",format );
		printf("  Endian: %s\n",format & AF_FORMAT_LE ? "LE" : "BE");
		printf("  Signed: %s\n",format & AF_FORMAT_US ? "No" : "Yes");
		printf("  Float: %s\n",format & AF_FORMAT_F ? "Yes" : "no");
		printf("  Bits: %d\n",bits[(format & AF_FORMAT_BITS_MASK)>>3] );
		printf("  Special Type: %d\n",(format & AF_FORMAT_SPECIAL_MASK) >> 6 );

		printf("Using Format AF_FORMAT_S16_BE\n" );

		ao_data.format =AF_FORMAT_S16_BE;
		convop  = CO_NONE;
		AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;

                mp_msg(MSGT_AO,MSGL_WARN,"AHI: Sound format %d not supported by this driver.\n", format);

		format = ao_data.format;


//		 return 0; // return fail !
	  }


   audio_bps = (float)ao_data.bps;
   if (format != AF_FORMAT_U8 && format != AF_FORMAT_S8)
   {
	 ao_data.bps*=2;
	 if (format == AF_FORMAT_S24_BE || format == AF_FORMAT_S24_LE || format == AF_FORMAT_S32_BE || format == AF_FORMAT_S32_LE)
	   ao_data.bps*=2;
   }

   if ( ! (buffer = AllocVec ( AHI_CHUNKMAX  , MEMF_PUBLIC | MEMF_CLEAR ) ) ) {
	  cleanup();
	  mp_msg(MSGT_AO, MSGL_WARN, "AHI: Not enough memory.\n");
	  return 0;
   }

	memset(&LockSemaphore, 0, sizeof(LockSemaphore));
	InitSemaphore(&LockSemaphore);

//Allocate our signal
	if ( ( sig_taskready = AllocSignal ( -1 ) )  == -1 )
	{
		cleanup();
		return 0;
	}

	//Allocate our signal
	if ( ( sig_taskend = AllocSignal ( -1 ) )  == -1 )
	{
   		cleanup();
		return 0;
	}

   kk(KPrintF("sig_taskready: %d |  sig_taskready %d\n", sig_taskready, sig_taskend );)

   MainTask = FindTask(NULL);

   kk(KPrintF("Creating Task\n");)

#ifdef __morphos__
  	PlayerTask_Process = CreateNewProcTags(
				 NP_CodeType,         CODETYPE_PPC,
				 NP_Entry,            (ULONG) PlayerTask,
				 NP_Name,             PLAYERTASK_NAME,
				 NP_Priority,         PLAYERTASK_PRIORITY,          
				 //NP_StartupMsg,     (ULONG) msg,
				 //NP_TaskMsgPort,    (ULONG) &port,
			  	TAG_DONE) ) ) 
#endif
#ifdef __amigaos4__	

#if debug_level > 0
	output = Open("CON:",MODE_READWRITE);	 
#endif

	PlayerTask_Process = CreateNewProcTags(
				NP_Name, PLAYERTASK_NAME ,
				NP_Entry, PlayerTask, 
				NP_Priority, PLAYERTASK_PRIORITY,        
#if debug_level >0
				NP_Output, output,  
#endif
				NP_Child, TRUE,
				TAG_END);
#endif

   if ( ! PlayerTask_Process )
  {
			cleanup();
			kk(KPrintF("AHI: Unable to create the audio process.\n");)
			mp_msg(MSGT_AO, MSGL_WARN, "AHI: Unable to create the audio process.\n");
			return 0;
   }

   // Here Wait for a singal from PlayerTask
   // It can bo "ok" -> sig_taskready
   // Or grrr ! -> sig_taskend
   kk(KPrintF("Waiting for signal for the PlayerTask\n");)
   if ( Wait ( (1L << sig_taskready) | (1L << sig_taskend) ) == (1L << sig_taskend) ) 
	{
		kk(KPrintF("Signal KO !\n");)
		cleanup();
		return 0; // Unable to init -> exit
	}
   kk(KPrintF("Signal received\n");)


	ao_data.buffersize=AHI_CHUNKMAX;
	ao_data.outburst = AHI_CHUNKSIZE;

   return 1;                                // Everything is ready to go !
}

/***************************** UNINIT ********************************/
// close audio device
static void ahi_dev_uninit(int immed)
{
	// send signal for process to quit.
	if (PlayerTask_Process)
	{
		Signal( &PlayerTask_Process->pr_Task, SIGBREAKF_CTRL_C );
	}

	Delay(1);

   kk(KPrintF("Freeing signals\n");)   
   if (sig_taskend != -1) {
	  if (PlayerTask_Process)
	  {
		Wait(SIGF_CHILD);
	  }
	  FreeSignal ( sig_taskend );   
	  sig_taskend=-1; 
   }
   if (sig_taskready != -1) {
	  FreeSignal (sig_taskready );
	  sig_taskready =-1;
   }
}

/***************************** RESET **********************************/
// stop playing and empty buffers (for seeking/pause)
static void reset()
{
   buffer_get=0;
   buffer_put=0;
   buffer_full=0;
   written_byte=0;
}

/***************************** PAUSE **********************************/
// stop playing, keep buffers (for pause)
static void audio_pause()
{
	paused = TRUE;
}

/***************************** RESUME **********************************/
// resume playing, after audio_pause()
static void audio_resume()
{
	paused = FALSE;
}

/***************************** GET_SPACE *******************************/
// return: how many bytes can be played without blocking
static int get_space(){
   return AHI_CHUNKMAX - written_byte; // aka total - used 
}

/***************************** PLAY ***********************************/
// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played

static int play(void* data,int len,int flags)
{
   int data_to_put, data_put;   
   int i, imax;
   LONG ltemp;	

   if(buffer == NULL) return 0;

//  gettimeofday(&tv_read,&dontcare);

   
   ObtainSemaphore(&LockSemaphore); // Semaphore to protect our variable as we have 2 tasks
 
   if ( buffer_full ) { 
	  ReleaseSemaphore(&LockSemaphore);
	  return 0;
   }
   
   data_to_put = buffer_get - buffer_put;
   if ( (data_to_put < 0) || ( (!data_to_put) && (!buffer_full) ) ) data_to_put = AHI_CHUNKMAX - buffer_put;
   if ( data_to_put > len) data_to_put = len ;
   data_put = data_to_put;

   CopyMem( data, buffer + buffer_put, data_to_put);    
   
   buffer_put += data_to_put;
   buffer_put %= AHI_CHUNKMAX;

   if (buffer_put==buffer_get) buffer_full = TRUE; // Our buffer is full !
   written_byte += data_to_put;

   tv_write = GetTimerMS();

   ReleaseSemaphore(&LockSemaphore); // We have finished to use varaibles which need protection
  
   return data_put;
}

/***************************** GET_DELAY **********************************/
// return: delay in seconds between first and last sample in buffer
static float get_delay()
{
	float t = tv_write ? (float)(GetTimerMS() - tv_write) / 1000.0f : 0.0f;
	return ((float)(written_byte)/(float)audio_bps) - t;
}
