/*****************************************************************************
 * device.h: DVD device access
 *****************************************************************************
 * Copyright (C) 1998-2006 VideoLAN
 * $Id: device.c 31156 2010-05-11 10:58:50Z diego $
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Sam Hocevar <sam@zoy.org>
 *          Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_ERRNO_H
#   include <errno.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_PARAM_H
#   include <sys/param.h>
#endif
#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifdef HAVE_LIMITS_H
#   include <limits.h>
#endif

#ifdef __amigaos4__
#include <proto/exec.h>
#include <proto/dos.h>
#include <devices/newstyle.h>

extern struct DebugIFace *IDebug;

#endif

//typedef char off_t;

#if defined( WIN32 ) && !defined( SYS_CYGWIN )
#   include <io.h>                                                 /* read() */
#else
#   include <sys/uio.h>                                      /* struct iovec */
#endif

#ifdef DARWIN_DVD_IOCTL
#   include <paths.h>
#   include <CoreFoundation/CoreFoundation.h>
#   include <IOKit/IOKitLib.h>
#   include <IOKit/IOBSD.h>
#   include <IOKit/storage/IOMedia.h>
#   include <IOKit/storage/IOCDMedia.h>
#   include <IOKit/storage/IODVDMedia.h>
#endif

#ifdef SYS_OS2
#   define INCL_DOS
#   define INCL_DOSDEVIOCTL
#   include <os2.h>
#   include <io.h>                                              /* setmode() */
#   include <fcntl.h>                                           /* O_BINARY  */
#endif

#include "dvdcss/dvdcss.h"

#include "common.h"
#include "css.h"
#include "libdvdcss.h"
#include "ioctl.h"
#include "device.h"

//--becnhmark--
static struct timezone dontcare = { 0,0 };
static struct timeval before, after;
static int bytes;

/*****************************************************************************
 * Device reading prototypes
 *****************************************************************************/
static int libc_open  ( dvdcss_t, char const * );
static int libc_seek  ( dvdcss_t, int );
static int libc_read  ( dvdcss_t, void *, int );
static int libc_readv ( dvdcss_t, struct iovec *, int );

#ifdef __amigaos4__
#define NUM_SECTORS    2
#define IPTR 		   uint32_t
#include <assert.h>
#include <proto/exec.h>
#include <devices/trackdisk.h>
#include <devices/scsidisk.h>

static uint32 local_fd = 0;
static dvdcss_t local_dvdcss = NULL;

#define evil_buffer_blocks 14
#define evil_buffer_size  (DVDCSS_BLOCK_SIZE*evil_buffer_blocks)

int64	evil_buffer_pos = -666;
void		*evil_buffer = NULL;
int		evil_result[ evil_buffer_blocks ];

static int read_from_buffer( dvdcss_t dvdcss, void *to_mem );

/*
	This is your hook function which will be called for each
	stack frame. Always check the State field before processing
	any stack frame.
*/

static int32 printStack(struct Hook *hook, struct Task *task, struct StackFrameMsg *frame);

int32 printStack(struct Hook *hook, struct Task *task, struct StackFrameMsg *frame)
{
	struct DebugSymbol *symbol = NULL;


	switch (frame->State)
	{
	    case STACK_FRAME_DECODED:


		if (symbol = IDebug->ObtainDebugSymbol(frame->MemoryAddress, NULL))
		{
			Printf("%s : %s\n", symbol -> Name, 
				symbol->SourceFunctionName ? symbol->SourceFunctionName : "NULL");
			IDebug->ReleaseDebugSymbol(symbol);
		}
		else
		{
			Printf("(%p) -> %p\n", frame->StackPointer, frame->MemoryAddress);
		}

		break;

	    case STACK_FRAME_INVALID_BACKCHAIN_PTR:
		Printf("(%p) invalid backchain pointer\n",  frame->StackPointer);
		break;

	    case STACK_FRAME_TRASHED_MEMORY_LOOP:
		Printf("(%p) trashed memory loop\n", frame->StackPointer);
	     	break;

	    case STACK_FRAME_BACKCHAIN_PTR_LOOP:
		Printf("(%p) backchain pointer loop\n",frame->StackPointer);
		break;

	    default:
		Printf("Unknown state=%lu\n", frame->State);
	}

	return 0;  // Continue tracing.
}


void dump_stack()
{
	int n=0;
	struct Task *task;

	n =n + 1 % 2000;
	if (n == 0) return;

	Printf("Stack:\n");
	
	if (task = local_dvdcss -> CurrentTask)
	{
		struct Hook *hook = AllocSysObjectTags(ASOT_HOOK,ASOHOOK_Entry, printStack, TAG_END);

		if (hook != NULL)
		{
			SuspendTask(task, 0);
			IDebug->StackTrace(task, hook);
			RestartTask(task, 0);
			FreeSysObject(ASOT_HOOK, hook);
		}
	}
}


static int hook(  void (*fn) ( dvdcss_t dvdcss, struct IOStdReq * DVD_IOReq, ULONG *args ), dvdcss_t dvdcss,uint64 arg1, uint64 arg2, uint64 arg3 );

static int amiga_open  ( dvdcss_t dvdcss, char const * device_name);
static void amiga_close(dvdcss_t dvdcss);
static void amiga_seek  ( dvdcss_t dvdcss, struct IOStdReq * DVD_IOReq, ULONG *args );
static void amiga_read  ( dvdcss_t dvdcss, struct IOStdReq * DVD_IOReq, ULONG *args );
static void amiga_readv ( dvdcss_t dvdcss, struct IOStdReq * DVD_IOReq, ULONG *args );

void StopDVDReader(dvdcss_t dvdcss);
int StartDVDReader(dvdcss_t dvdcss );

static void _AmigaOS_DoIO( dvdcss_t dvdcss, struct IOStdReq * DVD_IOReq, ULONG *args  );

static void evil_reader( dvdcss_t dvdcss, struct IOStdReq * DVD_IOReq,ULONG *args);

int AmigaOS_DoIO( ULONG i_fd, ULONG cmd, void *data, ULONG size)
{	
	return hook( _AmigaOS_DoIO , local_dvdcss, cmd, (ULONG) data, size );
}

static int _amiga_seek ( dvdcss_t dvdcss, int pos)
{
	return hook( amiga_seek, dvdcss, pos,0,0);
}

static int _amiga_read ( dvdcss_t dvdcss, void *data, int blocks )
{
//	return hook( amiga_read , dvdcss, (ULONG) data, blocks ,0 );

	if (blocks == 1)
	{
		if ( read_from_buffer( dvdcss, data )>0)
		{
			return 1;
		}
	}

	return hook( evil_reader , dvdcss, (ULONG) data, blocks ,0 );
}

static int _amiga_readv ( dvdcss_t dvdcss, struct iovec *iovec, int blocks)
{
	printf("*** readv **** %d, %d ****\n", iovec, blocks);
	return hook( amiga_readv , dvdcss, (ULONG) iovec, blocks, 0 );
}

static int hook(  void (*fn) ( dvdcss_t dvdcss, struct IOStdReq * DVD_IOReq, ULONG *args ), dvdcss_t dvdcss,uint64 arg1, uint64 arg2, uint64 arg3 )
{
	dvdcss -> CurrentTask = FindTask(NULL);  // find caller task/process.

	dvdcss -> args[0] = arg1;
	dvdcss -> args[1] = arg2;
	dvdcss -> args[2] = arg3;
	dvdcss -> hook = fn;
	Signal( &dvdcss -> Process->pr_Task, SIGBREAKF_CTRL_D );
	Wait(SIGF_CHILD);		// Wait for child process to signal its done.
	return dvdcss -> ret;
}

// prefetch buffer maybe...

static int read_from_buffer( dvdcss_t dvdcss, void *to_mem )
{
	int64 offset = dvdcss->i_pos % evil_buffer_blocks;
	int64 i_pos = dvdcss->i_pos - offset;

	if (evil_buffer_pos == i_pos)
	{
		if (evil_result[offset]>0)  
		{
			CopyMem( (void *)  (((ULONG) evil_buffer) + (ULONG) ( offset * DVDCSS_BLOCK_SIZE)), (void *) to_mem, DVDCSS_BLOCK_SIZE );
			dvdcss -> i_pos ++;
			return 1;
		}
	}

	return 0;
}

static void evil_reader( dvdcss_t dvdcss, struct IOStdReq * DVD_IOReq,ULONG *args)
{
	struct dvdcss_s private_dvdcss;
	ULONG send_args[3];
	ULONG offset;
	ULONG n =0;

	if ((args[1] != 1)||(dvdcss->i_pos < 0))
	{
		amiga_read( dvdcss, DVD_IOReq, args );
		return;
	}

	// get offeste and pos.
	
	offset = dvdcss->i_pos % evil_buffer_blocks;
	private_dvdcss.i_pos = dvdcss->i_pos - offset;

	if (evil_buffer_pos == private_dvdcss.i_pos)
	{
		// if prefetch buffer has vaild data return quick.
		if (evil_result[offset]>0)  
		{
			CopyMem( (void *)  (((ULONG) evil_buffer) + (ULONG) ( offset * DVDCSS_BLOCK_SIZE)), (void *) args[0], DVDCSS_BLOCK_SIZE );
			dvdcss -> i_pos ++;
			dvdcss -> ret = 1;
			return;
		}
	}
	else
	{
		for (n=0;n<evil_buffer_blocks;n++) 	evil_result[ n ] = -1;
	}

	// make private copy.
	CopyMem(dvdcss, &private_dvdcss, sizeof(struct dvdcss_s));
	// restore pos after CopyMem.

	offset = dvdcss->i_pos % evil_buffer_blocks;
	private_dvdcss.i_pos = dvdcss->i_pos - offset;

	send_args[0] = (ULONG) evil_buffer;
	send_args[1] = evil_buffer_blocks;
	amiga_read( &private_dvdcss, DVD_IOReq, send_args );

	if  (evil_result[ n ]==-1)
	{
		for (n=0;n<evil_buffer_blocks;n++) evil_result[n] = -1;
	}
	else
	{
		for (n=0;n<evil_buffer_blocks;n++) evil_result[n] = 1;
	}

	evil_buffer_pos = dvdcss->i_pos - offset;
	dvdcss -> ret = evil_result[ offset ];

	if (evil_result[ offset ] > -1)
	{
		CopyMem( (void *) (((ULONG) evil_buffer) + (ULONG) (offset * DVDCSS_BLOCK_SIZE )), (void *) args[0], DVDCSS_BLOCK_SIZE );
		dvdcss -> i_pos ++;
	}
	else
	{
		dvdcss -> i_pos=-1;
	}
}


#endif

#ifdef WIN32
static int win2k_open ( dvdcss_t, char const * );
static int aspi_open  ( dvdcss_t, char const * );
static int win2k_seek ( dvdcss_t, int );
static int aspi_seek  ( dvdcss_t, int );
static int win2k_read ( dvdcss_t, void *, int );
static int aspi_read  ( dvdcss_t, void *, int );
static int win_readv  ( dvdcss_t, struct iovec *, int );

static int aspi_read_internal  ( int, void *, int );
#elif defined( SYS_OS2 )
static int os2_open ( dvdcss_t, char const * );
/* just use macros for libc */
#   define os2_seek     libc_seek
#   define os2_read     libc_read
#   define os2_readv    libc_readv
#endif

int _dvdcss_use_ioctls( dvdcss_t dvdcss )
{
#if defined( WIN32 )
    if( dvdcss->b_file )
    {
        return 0;
    }

    /* FIXME: implement this for Windows */
    if( WIN2K )
    {
        return 1;
    }
    else
    {
        return 1;
    }
#elif defined(__amigaos4__)
 	return 1;
#elif defined( SYS_OS2 )
    ULONG ulMode;

    if( DosQueryFHState( dvdcss->i_fd, &ulMode ) != 0 )
        return 1;  /* What to do?  Be conservative and try to use the ioctls */

    if( ulMode & OPEN_FLAGS_DASD )
        return 1;

    return 0;
#else
    struct stat fileinfo;
    int ret;

    ret = fstat( dvdcss->i_fd, &fileinfo );
    if( ret < 0 )
    {
        return 1;  /* What to do?  Be conservative and try to use the ioctls */
    }

    /* Complete this list and check that we test for the right things
     * (I've assumed for all OSs that 'r', (raw) device, are char devices
     *  and those that don't contain/use an 'r' in the name are block devices)
     *
     * Linux    needs a block device
     * Solaris  needs a char device
     * Darwin   needs a char device
     * OpenBSD  needs a char device
     * NetBSD   needs a char device
     * FreeBSD  can use either the block or the char device
     * BSD/OS   can use either the block or the char device
     */

    /* Check if this is a block/char device */
    if( S_ISBLK( fileinfo.st_mode ) ||
        S_ISCHR( fileinfo.st_mode ) )
    {
        return 1;
    }
    else
    {
        return 0;
    }
#endif
}

void _dvdcss_check ( dvdcss_t dvdcss )
{
#if defined( WIN32 )
    DWORD drives;
    int i;
#elif defined( DARWIN_DVD_IOCTL )
    io_object_t next_media;
    mach_port_t master_port;
    kern_return_t kern_result;
    io_iterator_t media_iterator;
    CFMutableDictionaryRef classes_to_match;
#elif defined( SYS_OS2 )
#pragma pack( 1 )
    struct
    {
        BYTE bCmdInfo;
        BYTE bDrive;
    } param;

    struct
    {
        BYTE    abEBPB[31];
        USHORT  usCylinders;
        BYTE    bDevType;
        USHORT  usDevAttr;
    } data;
#pragma pack()

    ULONG ulParamLen;
    ULONG ulDataLen;
    ULONG rc;

    int i;
#else
    char *ppsz_devices[] = { "/dev/dvd", "/dev/cdrom", "/dev/hdc", NULL };
    int i, i_fd;
#endif

    /* If the device name is non-null, return */
    if( dvdcss->psz_device[0] )
    {
        return;
    }

#if defined( WIN32 )
    drives = GetLogicalDrives();

    for( i = 0; drives; i++ )
    {
        char psz_device[5];
        DWORD cur = 1 << i;
        UINT i_ret;

        if( (drives & cur) == 0 )
        {
            continue;
        }
        drives &= ~cur;

        sprintf( psz_device, "%c:\\", 'A' + i );
        i_ret = GetDriveType( psz_device );
        if( i_ret != DRIVE_CDROM )
        {
            continue;
        }

        /* Remove trailing backslash */
        psz_device[2] = '\0';

        /* FIXME: we want to differenciate between CD and DVD drives
         * using DeviceIoControl() */
        print_debug( dvdcss, "defaulting to drive `%s'", psz_device );
        free( dvdcss->psz_device );
        dvdcss->psz_device = strdup( psz_device );
        return;
    }
#elif defined( DARWIN_DVD_IOCTL )

    kern_result = IOMasterPort( MACH_PORT_NULL, &master_port );
    if( kern_result != KERN_SUCCESS )
    {
        return;
    }

    classes_to_match = IOServiceMatching( kIODVDMediaClass );
    if( classes_to_match == NULL )
    {
        return;
    }

    CFDictionarySetValue( classes_to_match, CFSTR( kIOMediaEjectableKey ),
                          kCFBooleanTrue );

    kern_result = IOServiceGetMatchingServices( master_port, classes_to_match,
                                                &media_iterator );
    if( kern_result != KERN_SUCCESS )
    {
        return;
    }

    next_media = IOIteratorNext( media_iterator );
    for( ; ; )
    {
        char psz_buf[0x32];
        size_t i_pathlen;
        CFTypeRef psz_path;

        next_media = IOIteratorNext( media_iterator );
        if( next_media == 0 )
        {
            break;
        }

        psz_path = IORegistryEntryCreateCFProperty( next_media,
                                                    CFSTR( kIOBSDNameKey ),
                                                    kCFAllocatorDefault,
                                                    0 );
        if( psz_path == NULL )
        {
            IOObjectRelease( next_media );
            continue;
        }

        snprintf( psz_buf, sizeof(psz_buf), "%s%c", _PATH_DEV, 'r' );
        i_pathlen = strlen( psz_buf );

        if( CFStringGetCString( psz_path,
                                (char*)&psz_buf + i_pathlen,
                                sizeof(psz_buf) - i_pathlen,
                                kCFStringEncodingASCII ) )
        {
            print_debug( dvdcss, "defaulting to drive `%s'", psz_buf );
            CFRelease( psz_path );
            IOObjectRelease( next_media );
            IOObjectRelease( media_iterator );
            free( dvdcss->psz_device );
            dvdcss->psz_device = strdup( psz_buf );
            return;
        }

        CFRelease( psz_path );

        IOObjectRelease( next_media );
    }

    IOObjectRelease( media_iterator );
#elif defined( SYS_OS2 )
    for( i = 0; i < 26; i++ )
    {
        param.bCmdInfo = 0;
        param.bDrive = i;

        rc = DosDevIOCtl( ( HFILE )-1, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                          &param, sizeof( param ), &ulParamLen,
                          &data, sizeof( data ), &ulDataLen );

        if( rc == 0 )
        {
            /* Check for removable and for cylinders */
            if( ( data.usDevAttr & 1 ) == 0 && data.usCylinders == 0xFFFF )
            {
                char psz_dvd[] = "A:";

                psz_dvd[0] += i;

                print_debug( dvdcss, "defaulting to drive `%s'", psz_dvd );
                free( dvdcss->psz_device );
                dvdcss->psz_device = strdup( psz_dvd );
                return;
            }
        }
    }
#else
    for( i = 0; ppsz_devices[i]; i++ )
    {
        i_fd = open( ppsz_devices[i], 0 );
        if( i_fd != -1 )
        {
            print_debug( dvdcss, "defaulting to drive `%s'", ppsz_devices[i] );
            close( i_fd );
            free( dvdcss->psz_device );
            dvdcss->psz_device = strdup( ppsz_devices[i] );
            return;
        }
    }
#endif

    print_error( dvdcss, "could not find a suitable default drive" );
}

int _dvdcss_open ( dvdcss_t dvdcss )
{
    char const *psz_device = dvdcss->psz_device;

    print_debug( dvdcss, "opening target `%s'", psz_device );

#if defined( WIN32 )
    dvdcss->b_file = 1;
    /* If device is "X:" or "X:\", we are not actually opening a file. */
    if (psz_device[0] && psz_device[1] == ':' &&
       (!psz_device[2] || (psz_device[2] == '\\' && !psz_device[3])))
        dvdcss->b_file = 0;

    /* Initialize readv temporary buffer */
    dvdcss->p_readv_buffer   = NULL;
    dvdcss->i_readv_buf_size = 0;

    if( !dvdcss->b_file && WIN2K )
    {
        print_debug( dvdcss, "using Win2K API for access" );
        dvdcss->pf_seek  = win2k_seek;
        dvdcss->pf_read  = win2k_read;
        dvdcss->pf_readv = win_readv;
        return win2k_open( dvdcss, psz_device );
    }
    else if( !dvdcss->b_file )
    {
        print_debug( dvdcss, "using ASPI for access" );
        dvdcss->pf_seek  = aspi_seek;
        dvdcss->pf_read  = aspi_read;
        dvdcss->pf_readv = win_readv;
        return aspi_open( dvdcss, psz_device );
    }
    else
#elif defined( SYS_OS2 )
    /* If device is "X:" or "X:\", we are not actually opening a file. */
    if( psz_device[0] && psz_device[1] == ':' &&
        ( !psz_device[2] || ( psz_device[2] == '\\' && !psz_device[3] ) ) )
    {
        print_debug( dvdcss, "using OS2 API for access" );
        dvdcss->pf_seek  = os2_seek;
        dvdcss->pf_read  = os2_read;
        dvdcss->pf_readv = os2_readv;
        return os2_open( dvdcss, psz_device );
    }
    else
#elif defined(__amigaos4__)
    {
        print_debug( dvdcss, "using AMIGAOS API for access" );

		dvdcss->p_readv_buffer   = NULL;
   		dvdcss->i_readv_buf_size = 0;

		dvdcss->pf_seek  = _amiga_seek;
		dvdcss->pf_read  = _amiga_read;
	  	dvdcss->pf_readv = _amiga_readv;
		return amiga_open( dvdcss, psz_device );
   }
#else
    {
        print_debug( dvdcss, "using libc for access" );
        dvdcss->pf_seek  = libc_seek;
        dvdcss->pf_read  = libc_read;
        dvdcss->pf_readv = libc_readv;
        return libc_open( dvdcss, psz_device );
    }
#endif
}

#if !defined(WIN32) && !defined(SYS_OS2)
int _dvdcss_raw_open ( dvdcss_t dvdcss, char const *psz_device )
{
    dvdcss->i_raw_fd = open( psz_device, 0 );

    if( dvdcss->i_raw_fd == -1 )
    {
        print_debug( dvdcss, "cannot open %s (%s)",
                             psz_device, strerror(errno) );
        print_error( dvdcss, "failed to open raw device, but continuing" );
        return -1;
    }
    else
    {
        dvdcss->i_read_fd = dvdcss->i_raw_fd;
    }

    return 0;
}
#endif

int _dvdcss_close ( dvdcss_t dvdcss )
{
#if defined( WIN32 )
    if( dvdcss->b_file )
    {
        close( dvdcss->i_fd );
    }
    else if( WIN2K )
    {
        CloseHandle( (HANDLE) dvdcss->i_fd );
    }
    else /* ASPI */
    {
        struct w32_aspidev *fd = (struct w32_aspidev *) dvdcss->i_fd;

        /* Unload aspi and free w32_aspidev structure */
        FreeLibrary( (HMODULE) fd->hASPI );
        free( (void*) dvdcss->i_fd );
    }

    /* Free readv temporary buffer */
    if( dvdcss->p_readv_buffer )
    {
        free( dvdcss->p_readv_buffer );
        dvdcss->p_readv_buffer   = NULL;
        dvdcss->i_readv_buf_size = 0;
    }

    return 0;
#elif defined(__amigaos4__)

	amiga_close( dvdcss );

   return 0;
#else
    close( dvdcss->i_fd );

#ifndef SYS_OS2
    if( dvdcss->i_raw_fd >= 0 )
    {
        close( dvdcss->i_raw_fd );
        dvdcss->i_raw_fd = -1;
    }
#endif

    return 0;
#endif
}

/* Following functions are local */

/*****************************************************************************
 * Open commands.
 *****************************************************************************/
static int libc_open ( dvdcss_t dvdcss, char const *psz_device )
{
#if !defined( WIN32 ) && !defined( SYS_OS2 )
    dvdcss->i_fd = dvdcss->i_read_fd = open( psz_device, 0 );
#else
    dvdcss->i_fd = dvdcss->i_read_fd = open( psz_device, O_BINARY );
#endif

    if( dvdcss->i_fd == -1 )
    {
        print_debug( dvdcss, "cannot open %s (%s)",
                             psz_device, strerror(errno) );
        print_error( dvdcss, "failed to open device" );
        return -1;
    }

    dvdcss->i_pos = 0;

    return 0;
}

#if defined( WIN32 )
static int win2k_open ( dvdcss_t dvdcss, char const *psz_device )
{
    char psz_dvd[7];
    _snprintf( psz_dvd, 7, "\\\\.\\%c:", psz_device[0] );

    /* To work around an M$ bug in IOCTL_DVD_READ_STRUCTURE, we need read
     * _and_ write access to the device (so we can make SCSI Pass Through
     * Requests). Unfortunately this is only allowed if you have
     * administrator priviledges so we allow for a fallback method with
     * only read access to the device (in this case ioctl_ReadCopyright()
     * won't send back the right result).
     * (See Microsoft Q241374: Read and Write Access Required for SCSI
     * Pass Through Requests) */
    dvdcss->i_fd = (int)
                CreateFile( psz_dvd, GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING,
                            FILE_FLAG_RANDOM_ACCESS, NULL );

    if( (HANDLE) dvdcss->i_fd == INVALID_HANDLE_VALUE )
        dvdcss->i_fd = (int)
                    CreateFile( psz_dvd, GENERIC_READ, FILE_SHARE_READ,
                                NULL, OPEN_EXISTING,
                                FILE_FLAG_RANDOM_ACCESS, NULL );

    if( (HANDLE) dvdcss->i_fd == INVALID_HANDLE_VALUE )
    {
        print_error( dvdcss, "failed opening device" );
        return -1;
    }

    dvdcss->i_pos = 0;

    return 0;
}

static int aspi_open( dvdcss_t dvdcss, char const * psz_device )
{
    HMODULE hASPI;
    DWORD dwSupportInfo;
    struct w32_aspidev *fd;
    int i, j, i_hostadapters;
    GETASPI32SUPPORTINFO lpGetSupport;
    SENDASPI32COMMAND lpSendCommand;
    char c_drive = psz_device[0];

    /* load aspi and init w32_aspidev structure */
    hASPI = LoadLibrary( "wnaspi32.dll" );
    if( hASPI == NULL )
    {
        print_error( dvdcss, "unable to load wnaspi32.dll" );
        return -1;
    }

    lpGetSupport = (GETASPI32SUPPORTINFO) GetProcAddress( hASPI, "GetASPI32SupportInfo" );
    lpSendCommand = (SENDASPI32COMMAND) GetProcAddress( hASPI, "SendASPI32Command" );

    if(lpGetSupport == NULL || lpSendCommand == NULL )
    {
        print_error( dvdcss, "unable to get aspi function pointers" );
        FreeLibrary( hASPI );
        return -1;
    }

    dwSupportInfo = lpGetSupport();

    if( HIBYTE( LOWORD ( dwSupportInfo ) ) == SS_NO_ADAPTERS )
    {
        print_error( dvdcss, "no ASPI adapters found" );
        FreeLibrary( hASPI );
        return -1;
    }

    if( HIBYTE( LOWORD ( dwSupportInfo ) ) != SS_COMP )
    {
        print_error( dvdcss, "unable to initalize aspi layer" );
        FreeLibrary( hASPI );
        return -1;
    }

    i_hostadapters = LOBYTE( LOWORD( dwSupportInfo ) );
    if( i_hostadapters == 0 )
    {
        print_error( dvdcss, "no ASPI adapters ready" );
        FreeLibrary( hASPI );
        return -1;
    }

    fd = malloc( sizeof( struct w32_aspidev ) );
    if( fd == NULL )
    {
        print_error( dvdcss, "not enough memory" );
        FreeLibrary( hASPI );
        return -1;
    }

    fd->i_blocks = 0;
    fd->hASPI = (long) hASPI;
    fd->lpSendCommand = lpSendCommand;

    c_drive = c_drive > 'Z' ? c_drive - 'a' : c_drive - 'A';

    for( i = 0; i < i_hostadapters; i++ )
    {
        for( j = 0; j < 15; j++ )
        {
            struct SRB_GetDiskInfo srbDiskInfo;

            srbDiskInfo.SRB_Cmd         = SC_GET_DISK_INFO;
            srbDiskInfo.SRB_HaId        = i;
            srbDiskInfo.SRB_Flags       = 0;
            srbDiskInfo.SRB_Hdr_Rsvd    = 0;
            srbDiskInfo.SRB_Target      = j;
            srbDiskInfo.SRB_Lun         = 0;

            lpSendCommand( (void*) &srbDiskInfo );

            if( (srbDiskInfo.SRB_Status == SS_COMP) &&
                (srbDiskInfo.SRB_Int13HDriveInfo == c_drive) )
            {
                /* Make sure this is a cdrom device */
                struct SRB_GDEVBlock srbGDEVBlock;

                memset( &srbGDEVBlock, 0, sizeof(struct SRB_GDEVBlock) );
                srbGDEVBlock.SRB_Cmd    = SC_GET_DEV_TYPE;
                srbGDEVBlock.SRB_HaId   = i;
                srbGDEVBlock.SRB_Target = j;

                lpSendCommand( (void*) &srbGDEVBlock );

                if( ( srbGDEVBlock.SRB_Status == SS_COMP ) &&
                    ( srbGDEVBlock.SRB_DeviceType == DTYPE_CDROM ) )
                {
                    fd->i_sid = MAKEWORD( i, j );
                    dvdcss->i_fd = (int) fd;
                    dvdcss->i_pos = 0;
                    return 0;
                }
                else
                {
                    free( (void*) fd );
                    FreeLibrary( hASPI );
                    print_error( dvdcss,"this is not a cdrom drive" );
                    return -1;
                }
            }
        }
    }

    free( (void*) fd );
    FreeLibrary( hASPI );
    print_error( dvdcss, "unable to get haid and target (aspi)" );
    return -1;
}
#endif

#ifdef SYS_OS2
static int os2_open ( dvdcss_t dvdcss, char const *psz_device )
{
    char  psz_dvd[] = "X:";
    HFILE hfile;
    ULONG ulAction;
    ULONG rc;

    psz_dvd[0] = psz_device[0];

    rc = DosOpenL( ( PSZ )psz_dvd, &hfile, &ulAction, 0, FILE_NORMAL,
                   OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW,
                   OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE | OPEN_FLAGS_DASD,
                   NULL );

    if( rc )
    {
        print_error( dvdcss, "failed to open device" );
        return -1;
    }

    setmode( hfile, O_BINARY );

    dvdcss->i_fd = dvdcss->i_read_fd = hfile;

    dvdcss->i_pos = 0;

    return 0;
}
#endif

/*****************************************************************************
 * Seek commands.
 *****************************************************************************/
static int libc_seek( dvdcss_t dvdcss, int i_blocks )
{
    off_t   i_seek;

    if( dvdcss->i_pos == i_blocks )
    {
        /* We are already in position */
        return i_blocks;
    }

    i_seek = (off_t)i_blocks * (off_t)DVDCSS_BLOCK_SIZE;
    i_seek = lseek( dvdcss->i_read_fd, i_seek, SEEK_SET );

    if( i_seek < 0 )
    {
        print_error( dvdcss, "seek error" );
        dvdcss->i_pos = -1;
        return i_seek;
    }

    dvdcss->i_pos = i_seek / DVDCSS_BLOCK_SIZE;

    return dvdcss->i_pos;
}

#if defined( WIN32 )
static int win2k_seek( dvdcss_t dvdcss, int i_blocks )
{
    LARGE_INTEGER li_seek;

#ifndef INVALID_SET_FILE_POINTER
#   define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif

    if( dvdcss->i_pos == i_blocks )
    {
        /* We are already in position */
        return i_blocks;
    }

    li_seek.QuadPart = (LONGLONG)i_blocks * DVDCSS_BLOCK_SIZE;

    li_seek.LowPart = SetFilePointer( (HANDLE) dvdcss->i_fd,
                                      li_seek.LowPart,
                                      &li_seek.HighPart, FILE_BEGIN );
    if( (li_seek.LowPart == INVALID_SET_FILE_POINTER)
        && GetLastError() != NO_ERROR)
    {
        dvdcss->i_pos = -1;
        return -1;
    }

    dvdcss->i_pos = li_seek.QuadPart / DVDCSS_BLOCK_SIZE;

    return dvdcss->i_pos;
}

static int aspi_seek( dvdcss_t dvdcss, int i_blocks )
{
    int i_old_blocks;
    char sz_buf[ DVDCSS_BLOCK_SIZE ];
    struct w32_aspidev *fd = (struct w32_aspidev *) dvdcss->i_fd;

    if( dvdcss->i_pos == i_blocks )
    {
        /* We are already in position */
        return i_blocks;
    }

    i_old_blocks = fd->i_blocks;
    fd->i_blocks = i_blocks;

    if( aspi_read_internal( dvdcss->i_fd, sz_buf, 1 ) == -1 )
    {
        fd->i_blocks = i_old_blocks;
        dvdcss->i_pos = -1;
        return -1;
    }

    (fd->i_blocks)--;

    dvdcss->i_pos = fd->i_blocks;

    return dvdcss->i_pos;
}
#endif

/*****************************************************************************
 * Read commands.
 *****************************************************************************/
static int libc_read ( dvdcss_t dvdcss, void *p_buffer, int i_blocks )
{
    off_t i_size, i_ret;

    i_size = (off_t)i_blocks * (off_t)DVDCSS_BLOCK_SIZE;
    i_ret = read( dvdcss->i_read_fd, p_buffer, i_size );

    if( i_ret < 0 )
    {
        print_error( dvdcss, "read error" );
        dvdcss->i_pos = -1;
        return i_ret;
    }

    /* Handle partial reads */
    if( i_ret != i_size )
    {
        int i_seek;

        dvdcss->i_pos = -1;
        i_seek = libc_seek( dvdcss, i_ret / DVDCSS_BLOCK_SIZE );
        if( i_seek < 0 )
        {
            return i_seek;
        }

        /* We have to return now so that i_pos isn't clobbered */
        return i_ret / DVDCSS_BLOCK_SIZE;
    }

    dvdcss->i_pos += i_ret / DVDCSS_BLOCK_SIZE;
    return i_ret / DVDCSS_BLOCK_SIZE;
}

#if defined( WIN32 )
static int win2k_read ( dvdcss_t dvdcss, void *p_buffer, int i_blocks )
{
    int i_bytes;

    if( !ReadFile( (HANDLE) dvdcss->i_fd, p_buffer,
              i_blocks * DVDCSS_BLOCK_SIZE,
              (LPDWORD)&i_bytes, NULL ) )
    {
        dvdcss->i_pos = -1;
        return -1;
    }

    dvdcss->i_pos += i_bytes / DVDCSS_BLOCK_SIZE;
    return i_bytes / DVDCSS_BLOCK_SIZE;
}

static int aspi_read ( dvdcss_t dvdcss, void *p_buffer, int i_blocks )
{
    int i_read = aspi_read_internal( dvdcss->i_fd, p_buffer, i_blocks );

    if( i_read < 0 )
    {
        dvdcss->i_pos = -1;
        return i_read;
    }

    dvdcss->i_pos += i_read;
    return i_read;
}
#endif

/*****************************************************************************
 * Readv commands.
 *****************************************************************************/
static int libc_readv ( dvdcss_t dvdcss, struct iovec *p_iovec, int i_blocks )
{
#if defined( WIN32 )
    int i_index, i_len, i_total = 0;
    unsigned char *p_base;
    int i_bytes;

    for( i_index = i_blocks;
         i_index;
         i_index--, p_iovec++ )
    {
        i_len  = p_iovec->iov_len;
        p_base = p_iovec->iov_base;

        if( i_len <= 0 )
        {
            continue;
        }

        i_bytes = read( dvdcss->i_fd, p_base, i_len );

        if( i_bytes < 0 )
        {
            /* One of the reads failed, too bad.
             * We won't even bother returning the reads that went ok,
             * and as in the posix spec the file postition is left
             * unspecified after a failure */
            dvdcss->i_pos = -1;
            return -1;
        }

        i_total += i_bytes;

        if( i_bytes != i_len )
        {
            /* We reached the end of the file or a signal interrupted
             * the read. Return a partial read. */
            int i_seek;

            dvdcss->i_pos = -1;
            i_seek = libc_seek( dvdcss, i_total / DVDCSS_BLOCK_SIZE );
            if( i_seek < 0 )
            {
                return i_seek;
            }

            /* We have to return now so that i_pos isn't clobbered */
            return i_total / DVDCSS_BLOCK_SIZE;
        }
    }

    dvdcss->i_pos += i_total / DVDCSS_BLOCK_SIZE;
    return i_total / DVDCSS_BLOCK_SIZE;
#else
    int i_read = readv( dvdcss->i_read_fd, p_iovec, i_blocks );

    if( i_read < 0 )
    {
        dvdcss->i_pos = -1;
        return i_read;
    }

    dvdcss->i_pos += i_read / DVDCSS_BLOCK_SIZE;
    return i_read / DVDCSS_BLOCK_SIZE;
#endif
}

#if defined( WIN32 )
/*****************************************************************************
 * win_readv: vectored read using ReadFile for Win2K and ASPI for win9x
 *****************************************************************************/
static int win_readv ( dvdcss_t dvdcss, struct iovec *p_iovec, int i_blocks )
{
    int i_index;
    int i_blocks_read, i_blocks_total = 0;

    /* Check the size of the readv temp buffer, just in case we need to
     * realloc something bigger */
    if( dvdcss->i_readv_buf_size < i_blocks * DVDCSS_BLOCK_SIZE )
    {
        dvdcss->i_readv_buf_size = i_blocks * DVDCSS_BLOCK_SIZE;

        if( dvdcss->p_readv_buffer ) free( dvdcss->p_readv_buffer );

        /* Allocate a buffer which will be used as a temporary storage
         * for readv */
        dvdcss->p_readv_buffer = malloc( dvdcss->i_readv_buf_size );
        if( !dvdcss->p_readv_buffer )
        {
            print_error( dvdcss, " failed (readv)" );
            dvdcss->i_pos = -1;
            return -1;
        }
    }

    for( i_index = i_blocks; i_index; i_index-- )
    {
        i_blocks_total += p_iovec[i_index-1].iov_len;
    }

    if( i_blocks_total <= 0 ) return 0;

    i_blocks_total /= DVDCSS_BLOCK_SIZE;

    if( WIN2K )
    {
        unsigned long int i_bytes;
        if( !ReadFile( (HANDLE)dvdcss->i_fd, dvdcss->p_readv_buffer,
                       i_blocks_total * DVDCSS_BLOCK_SIZE, &i_bytes, NULL ) )
        {
            /* The read failed... too bad.
             * As in the posix spec the file postition is left
             * unspecified after a failure */
            dvdcss->i_pos = -1;
            return -1;
        }
        i_blocks_read = i_bytes / DVDCSS_BLOCK_SIZE;
    }
    else /* Win9x */
    {
        i_blocks_read = aspi_read_internal( dvdcss->i_fd,
                                            dvdcss->p_readv_buffer,
                                            i_blocks_total );
        if( i_blocks_read < 0 )
        {
            /* See above */
            dvdcss->i_pos = -1;
            return -1;
        }
    }

    /* We just have to copy the content of the temp buffer into the iovecs */
    for( i_index = 0, i_blocks_total = i_blocks_read;
         i_blocks_total > 0;
         i_index++ )
    {
        memcpy( p_iovec[i_index].iov_base,
                dvdcss->p_readv_buffer + (i_blocks_read - i_blocks_total)
                                           * DVDCSS_BLOCK_SIZE,
                p_iovec[i_index].iov_len );
        /* if we read less blocks than asked, we'll just end up copying
         * garbage, this isn't an issue as we return the number of
         * blocks actually read */
        i_blocks_total -= ( p_iovec[i_index].iov_len / DVDCSS_BLOCK_SIZE );
    }

    dvdcss->i_pos += i_blocks_read;
    return i_blocks_read;
}

static int aspi_read_internal( int i_fd, void *p_data, int i_blocks )
{
    HANDLE hEvent;
    struct SRB_ExecSCSICmd ssc;
    struct w32_aspidev *fd = (struct w32_aspidev *) i_fd;

    /* Create the transfer completion event */
    hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    if( hEvent == NULL )
    {
        return -1;
    }

    memset( &ssc, 0, sizeof( ssc ) );

    ssc.SRB_Cmd         = SC_EXEC_SCSI_CMD;
    ssc.SRB_Flags       = SRB_DIR_IN | SRB_EVENT_NOTIFY;
    ssc.SRB_HaId        = LOBYTE( fd->i_sid );
    ssc.SRB_Target      = HIBYTE( fd->i_sid );
    ssc.SRB_SenseLen    = SENSE_LEN;

    ssc.SRB_PostProc = (LPVOID) hEvent;
    ssc.SRB_BufPointer  = p_data;
    ssc.SRB_CDBLen      = 12;

    ssc.CDBByte[0]      = 0xA8; /* RAW */
    ssc.CDBByte[2]      = (UCHAR) (fd->i_blocks >> 24);
    ssc.CDBByte[3]      = (UCHAR) (fd->i_blocks >> 16) & 0xff;
    ssc.CDBByte[4]      = (UCHAR) (fd->i_blocks >> 8) & 0xff;
    ssc.CDBByte[5]      = (UCHAR) (fd->i_blocks) & 0xff;

    /* We have to break down the reads into 64kb pieces (ASPI restriction) */
    if( i_blocks > 32 )
    {
        ssc.SRB_BufLen = 32 * DVDCSS_BLOCK_SIZE;
        ssc.CDBByte[9] = 32;
        fd->i_blocks  += 32;

        /* Initiate transfer */
        ResetEvent( hEvent );
        fd->lpSendCommand( (void*) &ssc );

        /* transfer the next 64kb (aspi_read_internal is called recursively)
         * We need to check the status of the read on return */
        if( aspi_read_internal( i_fd,
                                (uint8_t*) p_data + 32 * DVDCSS_BLOCK_SIZE,
                                i_blocks - 32) < 0 )
        {
            return -1;
        }
    }
    else
    {
        /* This is the last transfer */
        ssc.SRB_BufLen   = i_blocks * DVDCSS_BLOCK_SIZE;
        ssc.CDBByte[9]   = (UCHAR) i_blocks;
        fd->i_blocks += i_blocks;

        /* Initiate transfer */
        ResetEvent( hEvent );
        fd->lpSendCommand( (void*) &ssc );

    }

    /* If the command has still not been processed, wait until it's finished */
    if( ssc.SRB_Status == SS_PENDING )
    {
        WaitForSingleObject( hEvent, INFINITE );
    }
    CloseHandle( hEvent );

    /* check that the transfer went as planned */
    if( ssc.SRB_Status != SS_COMP )
    {
      return -1;
    }

    return i_blocks;
}
#endif

/*****************************************************/
/*****************************************************/
/*****************************************************/
/*              AMIGAOS4/AmigaOS section              */
/*****************************************************/
/*****************************************************/
/*****************************************************/
#ifdef __amigaos4__
#include "amiga_scsi.h"

struct MySCSICmd
{
	   struct SCSICmd req;
	   SCSICMD12 cmd;
};

static BOOL DiskPresent(struct IOStdReq *My_IOStdReq);
static BOOL read_sector (struct IOStdReq *My_IOStdReq, ULONG start_block, ULONG block_count, UBYTE *Data, struct MySCSICmd *MySCSI);
static UBYTE Global_SCSISense[SENSE_LEN];

/*******************/
//return 0 -> ok , -1 -> error
static int amiga_open  ( dvdcss_t dvdcss, char const * device_name)
{
	int success = -1;
	int sectors = (sizeof(struct MySCSICmd) + DVDCSS_BLOCK_SIZE + 31) * NUM_SECTORS;

	Printf("evil buffer size: %ld - block size %ld\n", DVDCSS_BLOCK_SIZE * (evil_buffer_size+2), DVDCSS_BLOCK_SIZE);

	evil_buffer_pos = -1;
	evil_buffer = AllocVec( DVDCSS_BLOCK_SIZE * (evil_buffer_size+2), MEMF_SHARED | MEMF_CLEAR );

	dvdcss -> Process = NULL;
	dvdcss->DVD_BufPtr = AllocVec(sectors, MEMF_SHARED | MEMF_CLEAR );
	dvdcss->DVD_Buffer   = (APTR)((((IPTR)dvdcss->DVD_BufPtr) + sizeof(struct MySCSICmd) + 31) & ~31);
	dvdcss->i_pos = 0;

	local_fd++;
	dvdcss->i_fd	= (int) local_fd;

	sscanf(device_name,"%[^:]:%ld", (char *) &dvdcss ->device_name, &dvdcss->device_unit);

	if ((dvdcss->DVD_BufPtr)&&(dvdcss->DVD_Buffer))
	{
		success = StartDVDReader( dvdcss );
	}
	
	if (success == -1)
	{
		if (dvdcss->DVD_BufPtr) FreeVec( dvdcss->DVD_BufPtr );
		if (dvdcss->DVD_Buffer) FreeVec( dvdcss->DVD_Buffer );
		dvdcss->DVD_BufPtr = NULL;
		dvdcss->DVD_Buffer = NULL;
		dvdcss->i_fd = 0;
	}

	return success;
}


static void amiga_close(dvdcss_t dvdcss)
{
//	Printf("%s:%d\n",__FUNCTION__,__LINE__);

	if (evil_buffer) FreeVec(evil_buffer);
	evil_buffer = NULL;

	StopDVDReader(dvdcss);

	if (dvdcss->DVD_BufPtr) FreeVec( dvdcss->DVD_BufPtr );
	if (dvdcss->DVD_Buffer) FreeVec( dvdcss->DVD_Buffer );
	dvdcss->DVD_BufPtr = NULL;
	dvdcss->DVD_Buffer = NULL;
	dvdcss->i_fd = 0;
}

/*****************************************************/

/*
static int libc_seek( dvdcss_t dvdcss, int i_blocks )
{
    off_t   i_seek;

    if( dvdcss->i_pos == i_blocks )
    {
        // We are already in position 
        return i_blocks;
    }

    i_seek = (off_t)i_blocks * (off_t)DVDCSS_BLOCK_SIZE;
    i_seek = lseek( dvdcss->i_read_fd, i_seek, SEEK_SET );

    if( i_seek < 0 )
    {
        print_error( dvdcss, "seek error" );
        dvdcss->i_pos = -1;
        return i_seek;
    }

    dvdcss->i_pos = i_seek / DVDCSS_BLOCK_SIZE;

    return dvdcss->i_pos;
}
*/

static void amiga_seek  ( dvdcss_t dvdcss, struct IOStdReq * DVD_IOReq , ULONG *args)
{
	int i_blocks =  args[0] ;		// +/- pos

	if (dvdcss->i_pos == i_blocks)
	{
		dvdcss -> ret = i_blocks;
		return;
	}

	if ( !read_sector(DVD_IOReq,i_blocks , 1, dvdcss->DVD_Buffer, dvdcss->DVD_BufPtr) )
	{
		dvdcss->i_pos = -1;
		dvdcss -> ret = -1;
		return ;
	}

	// can't seek to negative pos
	if (i_blocks<0)
	{
     		print_error( dvdcss, "seek error" );
		dvdcss->i_pos = -1;
		dvdcss -> ret = i_blocks * DVDCSS_BLOCK_SIZE;
		return;
	}

	dvdcss->i_pos = i_blocks;
	dvdcss -> ret = i_blocks;
}


/*****************************************************/

static BOOL read_sector_scsi (struct IOStdReq *My_IOStdReq, ULONG start_block, ULONG block_count, UBYTE *Data, struct MySCSICmd *MySCSI);


static int amiga_read_first_time = 1;
static uint64 amiga_read_time = 0;
static uint64 amiga_read_bytes = 0;

int amiga_read_cnt = 0;


static void amiga_read  ( dvdcss_t dvdcss, struct IOStdReq * DVD_IOReq, ULONG *args )
{
	void *p_buffer = (void *) args[0];
	int blocks =  args[1];

	if ( !read_sector(DVD_IOReq,dvdcss->i_pos , blocks, p_buffer, dvdcss->DVD_BufPtr) )
	{
		Printf("%s:%ld args %ld, %ld\n",__FUNCTION__,__LINE__, dvdcss->i_pos, dvdcss -> args[1]);
		Printf("error\n");
		dvdcss->i_pos = -1;
		dvdcss -> ret = -1;
		return ;
	}

	dvdcss->i_pos += blocks;
	dvdcss -> ret = blocks;
}


static void _AmigaOS_DoIO( dvdcss_t dvdcss, struct IOStdReq * DVD_IOReq, ULONG *args  )
{
	DVD_IOReq->io_Command	= args[0];
	DVD_IOReq->io_Data		= (void *) args[1];
	DVD_IOReq->io_Length		= args[2];

	DoIO((struct IORequest *) DVD_IOReq);

	dvdcss-> ret = DVD_IOReq->io_Error;
}


/*****************************************************/

static void amiga_readv ( dvdcss_t dvdcss, struct IOStdReq * DVD_IOReq, ULONG *args )
{
	struct iovec *p_iovec = (struct iovec *)  args[0];
	int i_blocks = args[1];
	int i_index;
	int i_blocks_read, i_blocks_total = 0;

	Printf("%s:%ld args %ld, %ld, %ld\n",__FUNCTION__,__LINE__, local_dvdcss -> args[0], local_dvdcss -> args[1], local_dvdcss -> args[2]);

    /* Check the size of the readv temp buffer, just in case we need to
     * realloc something bigger */

    if( local_dvdcss->i_readv_buf_size < i_blocks * DVDCSS_BLOCK_SIZE )
    {
        local_dvdcss->i_readv_buf_size = i_blocks * DVDCSS_BLOCK_SIZE;

        if( local_dvdcss->p_readv_buffer ) free( local_dvdcss->p_readv_buffer );

        /* Allocate a buffer which will be used as a temporary storage
         * for readv */

	Printf("%s:%ld - Alloced buffer :-)\n", __FUNCTION__,__LINE__);

        local_dvdcss->p_readv_buffer = malloc( local_dvdcss->i_readv_buf_size );

        if( !local_dvdcss->p_readv_buffer);
        {
		print_error( local_dvdcss, " failed (readv)" );
		local_dvdcss->i_pos = -1;
		local_dvdcss -> ret = -1;
		return ;
        }
    }


	Printf("%s:%ld - blocks is %ld\n", __FUNCTION__,__LINE__, i_blocks);

    for( i_index = i_blocks; i_index; i_index-- )
    {
        i_blocks_total += p_iovec[i_index-1].iov_len;
    }

	Printf("%s:%ld\n", __FUNCTION__,__LINE__);

	if( i_blocks_total <= 0 ) 
	{
		dvdcss -> ret = -1;
		return;
	}

	i_blocks_total /= DVDCSS_BLOCK_SIZE;

	dvdcss->args[0] = (ULONG) local_dvdcss->p_readv_buffer;
	dvdcss->args[1] = i_blocks_total;

	Printf("%s:%ld\n", __FUNCTION__,__LINE__);

	amiga_read( dvdcss, DVD_IOReq , local_dvdcss->args);
	i_blocks_read = dvdcss-> ret;


	Printf("%s:%ld\n", __FUNCTION__,__LINE__);

        if( i_blocks_read < 0 )
        {
		/* See above */
		local_dvdcss->i_pos = -1;
		local_dvdcss -> ret = -1;
		return;
	}

	Printf("%s:%ld\n", __FUNCTION__,__LINE__);

	/* We just have to copy the content of the temp buffer into the iovecs */
	for( i_index = 0, i_blocks_total = i_blocks_read;
		i_blocks_total > 0;
		i_index++ )
	{
		memcpy( p_iovec[i_index].iov_base,
			local_dvdcss->p_readv_buffer + (i_blocks_read - i_blocks_total) * DVDCSS_BLOCK_SIZE, p_iovec[i_index].iov_len );

        /* if we read less blocks than asked, we'll just end up copying
         * garbage, this isn't an issue as we return the number of
         * blocks actually read */

        i_blocks_total -= ( p_iovec[i_index].iov_len / DVDCSS_BLOCK_SIZE );
    }

    dvdcss->i_pos += i_blocks_read;

    dvdcss -> ret = i_blocks_read;
}



/*****************************************************/
static BOOL read_sector(struct IOStdReq *IOStdReq, ULONG start_block, ULONG block_count, UBYTE *data, struct MySCSICmd *MySCSI)
{
	long long int offset = (long long int) start_block * (long long int) DVDCSS_BLOCK_SIZE;

	IOStdReq->io_Command = NSCMD_TD_READ64;
	IOStdReq->io_Actual = offset >> 32;
	IOStdReq->io_Offset = offset;
	IOStdReq->io_Length = block_count * DVDCSS_BLOCK_SIZE;
	IOStdReq->io_Data = data;

	DoIO((struct IORequest *) IOStdReq);

	if (IOStdReq->io_Error)
	{
		printf("Error\n");
		return FALSE;
	}
	
	return TRUE;
}



static BOOL read_sector_scsi (struct IOStdReq *My_IOStdReq, ULONG start_block, ULONG block_count, UBYTE *Data, struct MySCSICmd *MySCSI)
{
	   MySCSI->cmd.opcode = SCSI_CMD_READ_CD12;
	   MySCSI->cmd.b1 = 0;
	   MySCSI->cmd.b2 = start_block >> 24;
	   MySCSI->cmd.b3 = (start_block >> 16) & 0xFF;
	   MySCSI->cmd.b4 = (start_block >> 8) & 0xFF;
	   MySCSI->cmd.b5 = start_block & 0xFF;

	   MySCSI->cmd.b6 = block_count >> 24;
	   MySCSI->cmd.b7 = (block_count >> 16) & 0xFF;
	   MySCSI->cmd.b8 = (block_count >> 8) & 0xFF;
	   MySCSI->cmd.b9 = block_count & 0xFF;

	   MySCSI->cmd.b10 = 0;
	   MySCSI->cmd.control = PAD;

	   MySCSI->req.scsi_Data 		   = (UWORD *) Data;
	   MySCSI->req.scsi_Length		   = block_count * DVDCSS_BLOCK_SIZE;
	   MySCSI->req.scsi_SenseActual    = 0;
	   MySCSI->req.scsi_SenseData	   = Global_SCSISense;
	   MySCSI->req.scsi_SenseLength	   = SENSE_LEN;
	   MySCSI->req.scsi_Command		   = (UBYTE *) &MySCSI->cmd;
	   MySCSI->req.scsi_CmdLength	   = sizeof(SCSICMD12);
	   MySCSI->req.scsi_Flags		   = SCSIF_READ | SCSIF_AUTOSENSE;

	   My_IOStdReq->io_Command    	   = HD_SCSICMD;
	   My_IOStdReq->io_Data       	   = &MySCSI->req;
	   My_IOStdReq->io_Length     	   = sizeof(struct SCSICmd);

	   DoIO((struct IORequest *) My_IOStdReq);

   return TRUE;

}
/*****************************************************/
static BOOL DiskPresent(struct IOStdReq *My_IOStdReq)
{
	   My_IOStdReq->io_Command    = TD_CHANGESTATE;
	   My_IOStdReq->io_Flags      = 0;

	   DoIO( (struct IORequest *)My_IOStdReq );

	   return My_IOStdReq->io_Actual ? FALSE : TRUE;
}

// -------------------------------------------------------------------------------
//  Can't use device IO across processes / threads on AmigaOS.
//  Need to force all IO on one process
// ------------------------------------------------------------------------------

void DVDReader()
{
	LONG DVD_Device;
	struct MsgPort *  DVD_MsgPort = NULL;
	struct IOStdReq * DVD_IOReq ;
	uint32 rsigs;
	struct Message *msg;

	Printf("DVD reader says hello\n");

	local_dvdcss -> ret = 0;

	Printf("try to setup msg port for iorequest\n");

	if (DVD_MsgPort = AllocSysObject(ASOT_PORT, NULL)) 
	{
		Printf("try to setup iorequest\n");

//		for (n=0;n<10;n++)
		{
			DVD_IOReq = AllocSysObjectTags(ASOT_IOREQUEST,
			     ASOIOR_Size, sizeof(struct IOStdReq),
			     ASOIOR_ReplyPort, DVD_MsgPort,
			     TAG_DONE);

//			DVD_IOReq->io_Message.mn_Node.ln_Type = 0;	// Avoid CheckIO() bug
		}
	} else
	{
		local_dvdcss -> ret = -1;
	}

	Printf("Time to try to open the device\n");

	if (local_dvdcss -> ret == 0)
	{
		Printf("Name %s  unit %ld\n",local_dvdcss ->device_name, local_dvdcss->device_unit);

		DVD_Device = OpenDevice(local_dvdcss ->device_name, local_dvdcss->device_unit, (struct IORequest *) DVD_IOReq, 0L);
		if (!DVD_Device)
		{
			Printf("device is open\n");

			if ( !DiskPresent( DVD_IOReq ) )
			{
				printf("No DVD present, go away !\n");
				local_dvdcss -> ret = -1;
			}

			Printf("try to do the first device read\n");

			if (local_dvdcss -> ret = 0)
			{
				read_sector(DVD_IOReq, 0, 1, local_dvdcss->DVD_Buffer, local_dvdcss->DVD_BufPtr);
			}
		} else
		{
			Printf("Can't open device\n");
			local_dvdcss -> ret = -1;
		}
	}

	if (local_dvdcss -> ret == 0)
	{

		Printf("all ok now, we can continue\n");
		Signal( local_dvdcss -> CurrentTask, SIGF_CHILD ); // every thing worked fine, so lets inform main program.

		for(;;)
		{	
//			Printf("waiting for some thing to do\n");
			rsigs = Wait(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | 1 << DVD_MsgPort -> mp_SigBit);

			if (rsigs & SIGBREAKF_CTRL_C) break;

			if (rsigs & SIGBREAKF_CTRL_D)
			{
//				dump_stack();
				local_dvdcss -> hook(local_dvdcss, DVD_IOReq,  (ULONG *) local_dvdcss -> args );
				Signal( local_dvdcss -> CurrentTask, SIGF_CHILD );
			}

			if (rsigs & (1 << DVD_MsgPort -> mp_SigBit))
			{
				while (msg = GetMsg(DVD_MsgPort))
				{
					Printf("Have a result from BeginIO\n");
					ReplyMsg(msg);
				}
			}
		}
	}

	Printf("Start shutdown\n");

	if (DVD_IOReq)
	{
		if(!CheckIO((struct IORequest *) DVD_IOReq))
			   AbortIO((struct IORequest *) DVD_IOReq );

		CloseDevice( (struct IORequest *) DVD_IOReq );
		FreeSysObject(ASOT_IOREQUEST, DVD_IOReq );
	}


	if (DVD_MsgPort) FreeSysObject(ASOT_PORT,DVD_MsgPort);


	// last thing to do wait up main task.
	Signal( local_dvdcss -> CurrentTask, SIGF_CHILD );
}


int StartDVDReader(dvdcss_t dvdcss )
{
//	ULONG output = Open("CON:",MODE_NEWFILE);

//	printf("start DVD reader process\n");

	local_dvdcss = dvdcss;

	dvdcss -> CurrentTask = FindTask(NULL);  // find caller task/process.

	dvdcss -> Process = CreateNewProcTags(
				NP_Name, "DVDReader" ,
				NP_Entry, DVDReader, 
//				NP_Output, output,
				NP_Priority, 0,        
				NP_Child, TRUE,
				TAG_END);

	Wait(SIGF_CHILD);		// Wait for child process to signal its done.
	printf("process started\n");


	return dvdcss -> ret;

}


void StopDVDReader(dvdcss_t dvdcss)
{
	if (dvdcss -> Process)
	{
		dvdcss -> CurrentTask = FindTask(NULL);
		Signal( &dvdcss -> Process->pr_Task, SIGBREAKF_CTRL_C );
		Wait(SIGF_CHILD);		// Wait for process to signal to continue
		Delay(1);				// I just wait and hope it has quit.
		dvdcss -> Process = NULL;
	}
}



#endif // __amigaos4__

